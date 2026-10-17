// ============================================================================
// tests/unit/set_ported_test.cpp -- the four PORTED AVL cores behind Collection::Set
// ============================================================================
// set_test.cpp drives Collection::Set over the GENERIC core (Collection.h compiles
// AvlGeneric for every width under BANISHED_UNIT_TEST). This file defines
// BANISHED_UNIT_PORTED_AVL before the header, which brings the ported `AvlOps`
// specialisations back for the element types declared HERE (they are this file's own, so
// no other test's Set changes), and links the four cores:
//
//   | element | node | core                                   | file                  |
//   |---------|------|----------------------------------------|-----------------------|
//   | 8       | 0x28 | RtColl_SetInsertFixup_1800274c0        | io/modification_avl8  |
//   | 0x10    | 0x30 | rtsetavl::Retrace / Erase (adc0, aba0) | collection/set_avl    |
//   | 0x18    | 0x38 | rtsetavl24::Retrace (0x180023170)      | collection/set_avl_e24|
//   | 0x30    | 0x50 | rtavl::Retrace50 (0x180030540)         | collection/avl_trees  |
//
// They were compiled out of the unit build because they stood on the address registry
// and the allocator singleton (Collection.h's banner); the registry is gone (2026-09-28)
// and the only remaining seam is the node free of width 0x10, which the test routes
// through the allocator's instrument seat. What is pinned:
//   * the specialisations ARE in effect: each width's Retrace is not AvlGeneric's;
//   * random insert / remove / find sequences per width, mirrored by a std::set, with
//     set_test.cpp's invariant walker (BST order, parent links, height, balance sign,
//     |balance| <= 1) after every operation, duplicates refused, Clear releasing every
//     node once;
//   * width 0x10 with the DefaultAllocator (the only way PortedErase16 is reached),
//     every node allocated and freed through the seat exactly once;
//   * the structural differential: the same sequence into a ported-width Set and into a
//     width-0x20 Set (no ported body: the generic core) leaves the same tree shape,
//     preorder (key, balance, height), after every operation. The generic core is the
//     transcription of the ported bodies; this is the check that nothing drifted.
// ============================================================================
#define BANISHED_UNIT_PORTED_AVL 1
#include "unit.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

#include "collection/Collection.h"
#include "system/MemoryAllocator.h"

namespace {

// ---------------------------------------------------------------------------
// The element types, one per width; THIS FILE'S OWN so the specialisations
// instantiate for these alone.
// ---------------------------------------------------------------------------
struct P8  { uint64_t k;                          bool operator<(const P8&  o) const { return k < o.k; } };
struct P16 { uint64_t k; uint64_t v;              bool operator<(const P16& o) const { return k < o.k; } };
struct P24 { uint64_t k; uint64_t b, c;           bool operator<(const P24& o) const { return k < o.k; } };
struct P48 { uint64_t k; uint64_t pad[5];         bool operator<(const P48& o) const { return k < o.k; } };
struct G32 { uint64_t k; uint64_t pad[3];         bool operator<(const G32& o) const { return k < o.k; } };  // no ported body

static_assert(sizeof(P8) == 8 && sizeof(P16) == 0x10 && sizeof(P24) == 0x18 && sizeof(P48) == 0x30 &&
              sizeof(G32) == 0x20, "one element type per width");

struct Counting {
    static int live, allocs, frees;
    static void Reset() { live = allocs = frees = 0; }
    static void* Allocate(uint64_t bytes, int align) {
        CHECK_EQ(align, 8);
        ++allocs; ++live;
        return ::operator new(static_cast<size_t>(bytes));
    }
    static void Free(void* p) { if (p) { ++frees; --live; } ::operator delete(p); }
};
int Counting::live = 0, Counting::allocs = 0, Counting::frees = 0;

template <class T, class A = Counting>
using PSet = Collection::Set<T, Collection::AVLCompare, A, T>;

// The instrument seat: the DefaultAllocator path (SetNodeAlloc / SetNodeFree, retail's
// 0x1800c7a50 / 0x180009c30) reaches MemoryAllocator::MemAlloc / MemFree, which consult
// the seat first; with a hook armed they never touch the (null) singleton.
int g_seatAllocs = 0, g_seatFrees = 0, g_seatLive = 0;
void* SeatAlloc(uint64_t size, int align) {
    CHECK_EQ(align, 8);
    ++g_seatAllocs; ++g_seatLive;
    return std::malloc(static_cast<size_t>(size));
}
void SeatFree(void* p) { if (p) { ++g_seatFrees; --g_seatLive; } std::free(p); }
struct SeatScope {
    SeatScope() {
        g_seatAllocs = g_seatFrees = g_seatLive = 0;
        System::g_memoryAllocatorInstrument.memAlloc = &SeatAlloc;
        System::g_memoryAllocatorInstrument.memFree  = &SeatFree;
    }
    ~SeatScope() {
        System::g_memoryAllocatorInstrument.memAlloc = nullptr;
        System::g_memoryAllocatorInstrument.memFree  = nullptr;
    }
};

// ---------------------------------------------------------------------------
// set_test.cpp's invariant walker, and a preorder shape dump.
// ---------------------------------------------------------------------------
struct WalkResult { int nodes = 0; bool ordered = true, linked = true, heights = true, balanced = true; };

template <class SetT>
int CheckSubtree(typename SetT::Node* n, typename SetT::Node* parent, WalkResult& r,
                 std::vector<uint64_t>& inorder) {
    if (!n) return -1;
    if (n->parent != parent) r.linked = false;
    ++r.nodes;
    const int lh = CheckSubtree<SetT>(n->left, n, r, inorder);
    inorder.push_back(n->element.k);
    const int rh = CheckSubtree<SetT>(n->right, n, r, inorder);
    const int height = (lh > rh ? lh : rh) + 1;
    const int balance = rh - lh;
    if (n->height != height || n->balance != balance) r.heights = false;
    if (balance < -1 || balance > 1) r.balanced = false;
    return height;
}
template <class SetT>
bool Sound(const SetT& s, int count) {
    WalkResult r;
    std::vector<uint64_t> inorder;
    if (s._root && s._root->parent != nullptr) r.linked = false;
    CheckSubtree<SetT>(s._root, nullptr, r, inorder);
    for (size_t i = 1; i < inorder.size(); ++i) if (!(inorder[i - 1] < inorder[i])) r.ordered = false;
    return r.ordered && r.linked && r.heights && r.balanced && r.nodes == count && s.GetCount() == count;
}

struct Shape { uint64_t k; int16_t balance, height; bool operator==(const Shape& o) const { return k == o.k && balance == o.balance && height == o.height; } };
template <class SetT>
void Preorder(typename SetT::Node* n, std::vector<Shape>& out) {
    if (!n) return;
    out.push_back(Shape{n->element.k, n->balance, n->height});
    Preorder<SetT>(n->left, out);
    Preorder<SetT>(n->right, out);
}

uint64_t NextRandom(uint64_t& st) { st ^= st >> 12; st ^= st << 25; st ^= st >> 27; return st * 2685821657736338717ull; }

template <class T> T Elem(uint64_t k) { T e; std::memset(&e, 0, sizeof(T)); e.k = k; return e; }

// One random sequence over `SetT`, mirrored by a std::set; every operation checked.
// Keys from a small pool so removes hit and duplicates happen. Returns the operations run.
template <class SetT, class T>
int Drive(SetT& s, uint64_t seed, int ops, int pool, std::vector<std::vector<Shape>>* shapes) {
    std::set<uint64_t> mirror;
    uint64_t st = seed * 0x9E3779B97F4A7C15ull + 1;
    int done = 0;
    for (int i = 0; i < ops; ++i) {
        const uint64_t r = NextRandom(st);
        const uint64_t k = (r >> 8) % static_cast<uint64_t>(pool);
        const int op = static_cast<int>(r & 3);          // 0, 1: insert; 2: remove; 3: find
        if (op < 2) {
            const bool fresh = mirror.insert(k).second;
            const int before = s.GetCount();
            typename SetT::Node* n = s.Insert(Elem<T>(k));
            CHECK(n != nullptr && n->element.k == k);
            CHECK_EQ(s.GetCount(), before + (fresh ? 1 : 0));   // a duplicate inserts nothing
        } else if (op == 2) {
            const bool had = mirror.erase(k) != 0;
            CHECK_EQ(s.Remove(Elem<T>(k)), had);
        } else {
            typename SetT::Node* n = s.Find(Elem<T>(k));
            CHECK_EQ(n != nullptr, mirror.count(k) != 0);
            if (n) CHECK_EQ(n->element.k, k);
        }
        CHECK(Sound(s, static_cast<int>(mirror.size())));
        if (shapes) { shapes->emplace_back(); Preorder<SetT>(s._root, shapes->back()); }
        ++done;
    }
    // Every mirrored key is found, no other key is.
    for (uint64_t k = 0; k < static_cast<uint64_t>(pool); ++k)
        CHECK_EQ(s.Find(Elem<T>(k)) != nullptr, mirror.count(k) != 0);
    return done;
}

template <class T>
void DriveWidth(const char* what) {
    for (uint64_t seed = 1; seed <= 4; ++seed) {
        Counting::Reset();
        {
            PSet<T> s;
            Drive<PSet<T>, T>(s, seed, 1500, 64, nullptr);
            CHECK(Counting::live == s.GetCount());
            s.Clear();
            CHECK_EQ(s.GetCount(), 0);
            CHECK(s._root == nullptr);
        }
        if (Counting::live != 0) std::printf("    %s seed %llu: %d nodes leaked\n", what,
                                             static_cast<unsigned long long>(seed), Counting::live);
        CHECK_EQ(Counting::live, 0);
        CHECK_EQ(Counting::allocs, Counting::frees);
    }
}

}  // namespace

// The specialisations are in effect: a ported width's Retrace is its own function, not
// AvlGeneric's; the width with no ported body (0x20) keeps the generic one.
UNIT_TEST(set_ported_specialisations_are_selected) {
    using namespace Collection::detail;
    using S8  = PSet<P8>;  using S16 = PSet<P16>; using S24 = PSet<P24>; using S48 = PSet<P48>; using S32 = PSet<G32>;
    CHECK((&AvlOps<S8::Node,  0x08>::template Retrace<S8>  != &AvlGeneric<S8::Node>::template Retrace<S8>));
    CHECK((&AvlOps<S16::Node, 0x10>::template Retrace<S16> != &AvlGeneric<S16::Node>::template Retrace<S16>));
    CHECK((&AvlOps<S24::Node, 0x18>::template Retrace<S24> != &AvlGeneric<S24::Node>::template Retrace<S24>));
    CHECK((&AvlOps<S48::Node, 0x30>::template Retrace<S48> != &AvlGeneric<S48::Node>::template Retrace<S48>));
    CHECK((&AvlOps<S32::Node, 0x20>::template Retrace<S32> == &AvlGeneric<S32::Node>::template Retrace<S32>));
    // Width 0x10's erase is the ported one only for retail's own free door.
    CHECK((&AvlOps<S16::Node, 0x10>::template Erase<S16> != &AvlGeneric<S16::Node>::template Erase<S16>));
}

UNIT_TEST(set_ported_width8_random_sequences)  { DriveWidth<P8>("width 8"); }
UNIT_TEST(set_ported_width16_random_sequences) { DriveWidth<P16>("width 0x10"); }
UNIT_TEST(set_ported_width24_random_sequences) { DriveWidth<P24>("width 0x18"); }
UNIT_TEST(set_ported_width48_random_sequences) { DriveWidth<P48>("width 0x30"); }

// Width 0x10 with the DefaultAllocator: PortedErase16 (0x18000aba0) runs, and frees the
// node through retail's own door (the seat here), each node once.
UNIT_TEST(set_ported_width16_default_allocator_erase) {
    SeatScope seat;
    for (uint64_t seed = 11; seed <= 14; ++seed) {
        using S = PSet<P16, Collection::DefaultAllocator>;
        {
            S s;
            Drive<S, P16>(s, seed, 1500, 48, nullptr);
            CHECK(g_seatLive == s.GetCount());
            // Remove every remaining key one by one (the ported erase on every shape).
            while (s._root) CHECK(s.Remove(s._root->element));
            CHECK_EQ(s.GetCount(), 0);
        }
        CHECK_EQ(g_seatLive, 0);
        CHECK_EQ(g_seatAllocs, g_seatFrees);
        CHECK(g_seatAllocs > 100);
        g_seatAllocs = g_seatFrees = 0;
    }
}

// The structural differential: ported width vs the generic core, the same tree after
// every operation.
namespace {
template <class T, class A = Counting>
void SameShapeAsGeneric(const char* what) {
    for (uint64_t seed = 21; seed <= 23; ++seed) {
        std::vector<std::vector<Shape>> ported, generic;
        {
            PSet<T, A> s;
            Drive<PSet<T, A>, T>(s, seed, 1200, 40, &ported);
            while (s._root) { s.Remove(s._root->element); ported.emplace_back(); Preorder<PSet<T, A>>(s._root, ported.back()); }
        }
        {
            PSet<G32> g;
            Drive<PSet<G32>, G32>(g, seed, 1200, 40, &generic);
            while (g._root) { g.Remove(g._root->element); generic.emplace_back(); Preorder<PSet<G32>>(g._root, generic.back()); }
        }
        CHECK_EQ(ported.size(), generic.size());
        size_t firstDiff = ported.size();
        for (size_t i = 0; i < ported.size() && i < generic.size(); ++i)
            if (!(ported[i] == generic[i])) { firstDiff = i; break; }
        if (firstDiff != ported.size())
            std::printf("    %s seed %llu: shape differs from the generic core after operation %zu\n",
                        what, static_cast<unsigned long long>(seed), firstDiff);
        CHECK_EQ(firstDiff, ported.size());
    }
}
}  // namespace

UNIT_TEST(set_ported_width8_matches_generic_shape)  { SameShapeAsGeneric<P8>("width 8"); }
UNIT_TEST(set_ported_width16_matches_generic_shape) { SameShapeAsGeneric<P16>("width 0x10"); }
UNIT_TEST(set_ported_width16_default_allocator_matches_generic_shape) {
    SeatScope seat;
    SameShapeAsGeneric<P16, Collection::DefaultAllocator>("width 0x10 (ported erase)");
    CHECK_EQ(g_seatLive, 0);
}
UNIT_TEST(set_ported_width24_matches_generic_shape) { SameShapeAsGeneric<P24>("width 0x18"); }
UNIT_TEST(set_ported_width48_matches_generic_shape) { SameShapeAsGeneric<P48>("width 0x30"); }
