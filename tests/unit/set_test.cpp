// ============================================================================
// tests/unit/set_test.cpp — Collection::Set (ENDGAME B.3.2 step 1, SET core)
// ============================================================================
//
// WHAT IS UNDER TEST, AND WHAT IS NOT
// -----------------------------------
// `Collection::Set<T, Compare, Allocator, Key>` is a WRAPPER: at runtime the
// rotations and the retrace are the Wave 9 bodies registered at
// 0x18000adc0/b040/b170/aba0 (element 0x10), 0x180023170/23570/236a0 (element
// 0x18) and 0x180030540/307b0/308e0 (element 0x30). Those bodies stand on the
// address registry and the allocator singleton, neither of which a host test
// links, so the header compiles its GENERIC core under `BANISHED_UNIT_TEST`
// (Collection.h's banner says so in as many words). The generic core is those
// same bodies with `Node*` in place of the baked-in offsets, so what this file
// verifies is the algorithm, the layouts, the duplicate-key rule and the
// ownership — and the real run verifies the ported bodies, which is why
// B32_RULES asks for one.
//
// No tree source is linked (tests/unit/sources.mk says the same): everything
// under test is in runtime/types/collection/Collection.h, and the allocator is
// the counting policy below — which is also how "Clear releases every node
// exactly once" is measured.
// ============================================================================

#include "unit.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "../../runtime/types/collection/Collection.h"

namespace {

// ---------------------------------------------------------------------------
// The counting allocator policy. `Collection::detail::AllocTraits` routes any
// non-DefaultAllocator policy straight at these two statics; DefaultAllocator
// is retail's 0x1800c7a50 / 0x180009c30 pair and is not reachable here.
// ---------------------------------------------------------------------------
struct CountingAllocator {
    static int  live;
    static int  allocs;
    static int  frees;
    static void Reset() { live = 0; allocs = 0; frees = 0; }
    static void* Allocate(uint64_t bytes, int align) {
        CHECK_EQ(align, 8);                      // Set nodes are alignment 8
        ++allocs; ++live;
        return ::operator new(static_cast<size_t>(bytes));
    }
    static void Free(void* p) {
        if (p) { ++frees; --live; }
        ::operator delete(p);
    }
};
int CountingAllocator::live   = 0;
int CountingAllocator::allocs = 0;
int CountingAllocator::frees  = 0;

// ---------------------------------------------------------------------------
// One element type per node width. The widths are the three the DLL evidence
// covers (Collection.h's LayoutProbe): 8 -> node 0x28, 0x10 -> node 0x30,
// 0x18 -> node 0x38.
// ---------------------------------------------------------------------------
struct E8 {                                  // exe EntityList tree element
    uint64_t k;
    bool operator<(const E8& o) const { return k < o.k; }
};
struct Pair16 {                              // Map_Insert's {key, value} pair
    uint64_t key;
    uint64_t value;
    bool operator<(const Pair16& o) const { return key < o.key; }
};
struct E24 {                                 // Input::KeyMap's key-set element
    uint64_t k;
    uint64_t b;
    uint64_t c;
    bool operator<(const E24& o) const { return k < o.k; }
};

// The key of a Pair element is its LEADING SUB-OBJECT, which is retail's rule
// (Collection.h: "Key == T: the key is the element or its leading
// sub-object"). Spelling the Key parameter proves the template honours it.
struct U64Key {
    uint64_t k;
    bool operator<(const U64Key& o) const { return k < o.k; }
};
struct Pair16Keyed {
    U64Key   key;
    uint64_t value;
};

template <class T, class Key = T>
using TestSet = Collection::Set<T, Collection::AVLCompare, CountingAllocator, Key>;

// ---------------------------------------------------------------------------
// The test-only AVL walker. It reads the PUBLIC node layout only (element,
// parent, left, right, balance, height) and checks every invariant the retrace
// is supposed to maintain:
//
//   * BST order (in-order keys strictly increasing)
//   * parent linkage on every edge, and a null parent at the root
//   * height  == max(childHeight + 1), a leaf being 0 (set_avl.cpp's Recompute)
//   * balance == rightHeight - leftHeight   *** NOTE THE SIGN ***
//   * |balance| <= 1 everywhere — the AVL invariant itself
// ---------------------------------------------------------------------------
struct WalkResult {
    int  nodes      = 0;
    bool ordered    = true;
    bool linked     = true;
    bool heights    = true;
    bool balanced   = true;
};

template <class SetT, class KeyOf>
int CheckSubtree(typename SetT::Node* n, typename SetT::Node* parent,
                 WalkResult& r, KeyOf keyOf,
                 std::vector<uint64_t>& inorder) {
    if (!n) return -1;                       // a null child's "height + 1" is 0
    if (n->parent != parent) r.linked = false;
    ++r.nodes;

    const int lh = CheckSubtree<SetT>(n->left, n, r, keyOf, inorder);
    inorder.push_back(keyOf(n->element));
    const int rh = CheckSubtree<SetT>(n->right, n, r, keyOf, inorder);

    const int height  = (lh > rh ? lh : rh) + 1;
    const int balance = rh - lh;
    if (n->height  != height)  r.heights  = false;
    if (n->balance != balance) r.heights  = false;
    if (balance < -1 || balance > 1) r.balanced = false;
    return height;
}

template <class SetT, class KeyOf>
WalkResult Walk(const SetT& s, KeyOf keyOf) {
    WalkResult r;
    std::vector<uint64_t> inorder;
    if (s._root && s._root->parent != nullptr) r.linked = false;
    CheckSubtree<SetT>(s._root, nullptr, r, keyOf, inorder);
    for (size_t i = 1; i < inorder.size(); ++i)
        if (!(inorder[i - 1] < inorder[i])) r.ordered = false;
    return r;
}

auto KeyOfE8     = [](const E8& e)     { return e.k; };
auto KeyOfPair16 = [](const Pair16& e) { return e.key; };
auto KeyOfE24    = [](const E24& e)    { return e.k; };

// A deterministic shuffle, so a failure is reproducible. (xorshift64*)
uint64_t NextRandom(uint64_t& state) {
    state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
    return state * 2685821657736338717ull;
}

}  // namespace

// ============================================================================
// Layout — the same three widths Collection.h's LayoutProbe pins, checked
// through the CLASS this time (the class must not have moved one byte).
// ============================================================================
UNIT_TEST(set_layout_is_unchanged) {
    using S8  = TestSet<E8>;
    using S16 = TestSet<Pair16>;
    using S24 = TestSet<E24>;

    static_assert(sizeof(S8) == 0x10, "Set is {root, count} + 4 bytes of slack");
    static_assert(offsetof(S8, _root)  == 0x00, "Set::_root @ +0x00");
    static_assert(offsetof(S8, _count) == 0x08, "Set::_count @ +0x08");

    static_assert(sizeof(S8::Node)  == 0x28, "Set::Node(T=8) is 0x28");
    static_assert(sizeof(S16::Node) == 0x30, "Set::Node(T=0x10) is 0x30");
    static_assert(sizeof(S24::Node) == 0x38, "Set::Node(T=0x18) is 0x38");

    static_assert(offsetof(S8::Node,  parent) == 0x08, "T=8 parent @ +0x08");
    static_assert(offsetof(S16::Node, parent) == 0x10, "T=0x10 parent @ +0x10");
    static_assert(offsetof(S24::Node, parent) == 0x18, "T=0x18 parent @ +0x18");
    static_assert(offsetof(S16::Node, left)    == 0x18, "T=0x10 left @ +0x18");
    static_assert(offsetof(S16::Node, right)   == 0x20, "T=0x10 right @ +0x20");
    static_assert(offsetof(S16::Node, balance) == 0x28, "T=0x10 balance @ +0x28");
    static_assert(offsetof(S16::Node, height)  == 0x2a, "T=0x10 height @ +0x2a");

    CHECK(sizeof(typename S8::Iterator) == 8);   // one node pointer, nothing else
}

// ============================================================================
// Insert / Find / Remove, once per node width.
// ============================================================================
UNIT_TEST(set_insert_find_remove_width8) {
    CountingAllocator::Reset();
    {
        TestSet<E8> s;
        for (uint64_t i = 0; i < 64; ++i) CHECK(s.Insert(E8{i}) != nullptr);
        CHECK_EQ(s.GetCount(), 64);
        CHECK_EQ(CountingAllocator::live, 64);

        for (uint64_t i = 0; i < 64; ++i) {
            typename TestSet<E8>::Node* n = s.Find(E8{i});
            CHECK(n != nullptr);
            if (n) CHECK_EQ(n->element.k, i);
        }
        CHECK(s.Find(E8{64}) == nullptr);

        CHECK(s.Remove(E8{31}));
        CHECK_EQ(s.GetCount(), 63);
        CHECK(s.Find(E8{31}) == nullptr);
        CHECK(!s.Remove(E8{31}));            // already gone
        WalkResult r = Walk(s, KeyOfE8);
        CHECK_EQ(r.nodes, 63);
        CHECK(r.ordered && r.linked && r.heights && r.balanced);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

UNIT_TEST(set_insert_find_remove_width16_pair) {
    CountingAllocator::Reset();
    {
        TestSet<Pair16> s;
        for (uint64_t i = 0; i < 64; ++i) s.Insert(Pair16{i, i * 10});
        CHECK_EQ(s.GetCount(), 64);
        for (uint64_t i = 0; i < 64; ++i) {
            typename TestSet<Pair16>::Node* n = s.Find(Pair16{i, 0});
            CHECK(n != nullptr);
            if (n) CHECK_EQ(n->element.value, i * 10);   // the VALUE came along
        }
        // Erase every third key and re-check the invariants.
        for (uint64_t i = 0; i < 64; i += 3) CHECK(s.Remove(Pair16{i, 0}));
        WalkResult r = Walk(s, KeyOfPair16);
        CHECK_EQ(r.nodes, s.GetCount());
        CHECK(r.ordered && r.linked && r.heights && r.balanced);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

UNIT_TEST(set_insert_find_remove_width24) {
    CountingAllocator::Reset();
    {
        TestSet<E24> s;
        for (uint64_t i = 0; i < 64; ++i) s.Insert(E24{i, i + 1, i + 2});
        CHECK_EQ(s.GetCount(), 64);
        for (uint64_t i = 0; i < 64; ++i) {
            typename TestSet<E24>::Node* n = s.Find(E24{i, 0, 0});
            CHECK(n != nullptr);
            if (n) CHECK_EQ(n->element.c, i + 2);
        }
        for (uint64_t i = 1; i < 64; i += 2) CHECK(s.Remove(E24{i, 0, 0}));
        WalkResult r = Walk(s, KeyOfE24);
        CHECK_EQ(r.nodes, 32);
        CHECK(r.ordered && r.linked && r.heights && r.balanced);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

// ============================================================================
// The duplicate-key rule — retail's equal arm (0x180030418 -> 0x180030443):
// *** the EXISTING node comes back, nothing is inserted, the count does not
// move, and no node is allocated. ***
// ============================================================================
UNIT_TEST(set_duplicate_key_returns_the_existing_node) {
    CountingAllocator::Reset();
    {
        TestSet<Pair16> s;
        typename TestSet<Pair16>::Node* first = s.Insert(Pair16{7, 700});
        CHECK(first != nullptr);
        CHECK_EQ(s.GetCount(), 1);
        const int allocsAfterFirst = CountingAllocator::allocs;

        typename TestSet<Pair16>::Node* again = s.Insert(Pair16{7, 999});
        CHECK(again == first);                          // the SAME node
        CHECK_EQ(s.GetCount(), 1);                      // count did not move
        CHECK_EQ(CountingAllocator::allocs, allocsAfterFirst);   // nothing allocated
        CHECK_EQ(again->element.value, 700u);           // the value is NOT updated

        // ... and it holds against a root insert too (the empty-tree arm is a
        // different branch of the core).
        for (uint64_t i = 0; i < 32; ++i) s.Insert(Pair16{i, i});
        const int32_t count = s.GetCount();
        const int allocs = CountingAllocator::allocs;
        for (uint64_t i = 0; i < 32; ++i) {
            typename TestSet<Pair16>::Node* n = s.Insert(Pair16{i, 12345});
            CHECK(n == s.Find(Pair16{i, 0}));
        }
        CHECK_EQ(s.GetCount(), count);
        CHECK_EQ(CountingAllocator::allocs, allocs);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

// ============================================================================
// A Pair whose Key is its LEADING SUB-OBJECT, spelled as the Key parameter —
// retail's `Set<Pair<K,V>, …, K, Compare>` shape, which is the only "Map" this
// engine has.
// ============================================================================
UNIT_TEST(set_pair_keyed_by_leading_subobject) {
    CountingAllocator::Reset();
    {
        TestSet<Pair16Keyed, U64Key> s;
        for (uint64_t i = 0; i < 32; ++i) s.Insert(Pair16Keyed{U64Key{i}, i * 3});
        CHECK_EQ(s.GetCount(), 32);
        for (uint64_t i = 0; i < 32; ++i) {
            typename TestSet<Pair16Keyed, U64Key>::Node* n = s.Find(U64Key{i});
            CHECK(n != nullptr);
            if (n) CHECK_EQ(n->element.value, i * 3);
        }
        CHECK(s.Find(U64Key{99}) == nullptr);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

// ============================================================================
// In-order iteration, through the Iterator (the parent-pointer successor walk
// inlined into 0x1800a76c0).
// ============================================================================
UNIT_TEST(set_iterates_in_key_order) {
    CountingAllocator::Reset();
    {
        TestSet<E8> s;
        CHECK(s.Begin() == s.End());                    // empty
        uint64_t state = 0x9e3779b97f4a7c15ull;
        for (int i = 0; i < 200; ++i) s.Insert(E8{NextRandom(state) % 1000});

        int seen = 0;
        uint64_t last = 0;
        for (typename TestSet<E8>::Iterator it = s.Begin(); it != s.End(); ++it) {
            if (seen) CHECK(last < (*it).k);            // strictly increasing
            last = (*it).k;
            ++seen;
        }
        CHECK_EQ(seen, s.GetCount());
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

// ============================================================================
// Clear releases every node EXACTLY ONCE, and it runs the element destructor
// exactly once per element (which is what the string-map instantiation's
// `~Pair<String,String>` rides on — set_clear.cpp).
// ============================================================================
namespace {
int g_dtorCalls = 0;
struct Counted {
    uint64_t k;
    uint64_t pad;                                   // element width 0x10
    Counted() : k(0), pad(0) {}
    explicit Counted(uint64_t v) : k(v), pad(0) {}
    Counted(const Counted& o) : k(o.k), pad(o.pad) {}
    ~Counted() { ++g_dtorCalls; }
    bool operator<(const Counted& o) const { return k < o.k; }
};
}  // namespace

UNIT_TEST(set_clear_releases_every_node_once) {
    CountingAllocator::Reset();
    g_dtorCalls = 0;
    {
        Collection::Set<Counted, Collection::AVLCompare, CountingAllocator> s;
        for (uint64_t i = 0; i < 100; ++i) s.Insert(Counted(i));
        CHECK_EQ(s.GetCount(), 100);
        CHECK_EQ(CountingAllocator::live, 100);
        const int dtorsFromTemporaries = g_dtorCalls;   // the `Counted(i)` args

        s.Clear();
        CHECK_EQ(s.GetCount(), 0);
        CHECK(s.GetRoot() == nullptr);
        CHECK_EQ(CountingAllocator::live, 0);
        CHECK_EQ(CountingAllocator::frees, 100);        // exactly once each
        CHECK_EQ(g_dtorCalls - dtorsFromTemporaries, 100);

        // Retail's Clear writes the two zeroes even on an already-empty Set
        // (it branches to a second copy of them at 0x18000f824 rather than
        // skipping); a second Clear must therefore be a no-op, not a crash.
        s.Clear();
        CHECK_EQ(s.GetCount(), 0);
        CHECK_EQ(CountingAllocator::frees, 100);

        s.Insert(Counted(1));                           // usable again after Clear
        CHECK_EQ(s.GetCount(), 1);
    }
    CHECK_EQ(CountingAllocator::live, 0);               // the destructor clears too
    CHECK_EQ(CountingAllocator::frees, 101);
}

// ============================================================================
// Balance after ascending, descending and random inserts — the retrace is the
// only thing standing between an AVL tree and a linked list, and AVL DEFECTS
// CANCEL (set_avl.cpp's ORACLE banner), so the walker checks shape, height,
// balance and parent linkage at every node rather than just the key sequence.
// ============================================================================
UNIT_TEST(set_balances_ascending_inserts) {
    CountingAllocator::Reset();
    {
        TestSet<E8> s;
        for (uint64_t i = 0; i < 1000; ++i) s.Insert(E8{i});
        WalkResult r = Walk(s, KeyOfE8);
        CHECK_EQ(r.nodes, 1000);
        CHECK(r.ordered && r.linked && r.heights && r.balanced);
        // An AVL tree of 1000 nodes is at most 1.44*log2(1001) ~= 14 deep; a
        // retrace that never ran would leave a 1000-deep spine.
        CHECK(s.GetRoot()->height <= 14);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

UNIT_TEST(set_balances_descending_inserts) {
    CountingAllocator::Reset();
    {
        TestSet<Pair16> s;
        for (uint64_t i = 1000; i > 0; --i) s.Insert(Pair16{i, i});
        WalkResult r = Walk(s, KeyOfPair16);
        CHECK_EQ(r.nodes, 1000);
        CHECK(r.ordered && r.linked && r.heights && r.balanced);
        CHECK(s.GetRoot()->height <= 14);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

UNIT_TEST(set_balances_random_inserts_and_removes) {
    CountingAllocator::Reset();
    {
        TestSet<E24> s;
        uint64_t state = 12345;
        for (int i = 0; i < 2000; ++i) s.Insert(E24{NextRandom(state) % 5000, 0, 0});
        WalkResult r = Walk(s, KeyOfE24);
        CHECK_EQ(r.nodes, s.GetCount());
        CHECK(r.ordered && r.linked && r.heights && r.balanced);

        // Erase half of the key space, re-checking the invariants throughout —
        // the erase's case 3 is a PREDECESSOR splice and a successor splice
        // would leave a valid tree with the WRONG SHAPE, which only a
        // shape-checking walk catches.
        for (uint64_t k = 0; k < 5000; k += 2) s.Remove(E24{k, 0, 0});
        r = Walk(s, KeyOfE24);
        CHECK_EQ(r.nodes, s.GetCount());
        CHECK(r.ordered && r.linked && r.heights && r.balanced);
        for (uint64_t k = 0; k < 5000; k += 2) CHECK(s.Find(E24{k, 0, 0}) == nullptr);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}

// ============================================================================
// A POINTER KEY: null orders as less than everything, and it does so in the
// DESCENT, not in the comparator — retail's two null arms are asymmetric (a
// null SEARCH key goes LEFT, a null NODE key goes RIGHT: avl_trees.cpp's
// group-B banner) and a "tidied" comparator that collapsed them would build a
// differently-shaped tree on any input containing a null key.
//
// `Collection::AVLCompare` on a `char*` key resolves to
// `detail::AvlStrCmpSigned`, which is out of line in set_core.cpp and not
// linked here, so the test supplies its own comparator policy. What it
// exercises is `KeyTraits<char*>` and the two null arms, which are the header's.
// ============================================================================
namespace {
struct StrCompare {
    static bool Less(char* const& a, char* const& b) {
        return std::strcmp(a, b) < 0;      // both non-null: the null arms are the
    }                                      // descent's job, not the comparator's
};
struct Row { char* key; uint64_t value; };
using RowSet = Collection::Set<Row, StrCompare, CountingAllocator, char*>;
}  // namespace

UNIT_TEST(set_pointer_key_null_orders_first) {
    CountingAllocator::Reset();
    {
        RowSet s;
        char alpha[] = "alpha", beta[] = "beta", gamma[] = "gamma";
        s.Insert(Row{beta,  2});
        s.Insert(Row{alpha, 1});
        s.Insert(Row{gamma, 3});
        s.Insert(Row{nullptr, 99});                 // the null key
        CHECK_EQ(s.GetCount(), 4);

        // In-order: the null key first, then the three strings in order.
        RowSet::Iterator it = s.Begin();
        CHECK((*it).key == nullptr);
        ++it; CHECK_STREQ((*it).key, "alpha");
        ++it; CHECK_STREQ((*it).key, "beta");
        ++it; CHECK_STREQ((*it).key, "gamma");
        ++it; CHECK(it == s.End());

        CHECK(s.Find(beta) != nullptr);
        CHECK(s.Find(nullptr) != nullptr);
        char* absent = const_cast<char*>("delta");
        CHECK(s.Find(absent) == nullptr);

        // A duplicate null key is still a lookup, not an insert.
        CHECK(s.Insert(Row{nullptr, 1234}) == s.Find(nullptr));
        CHECK_EQ(s.GetCount(), 4);
    }
    CHECK_EQ(CountingAllocator::live, 0);
}
