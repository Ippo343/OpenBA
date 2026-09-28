// ============================================================================
// tests/unit/b39_eng_collision2d_test.cpp — ENDGAME B.3.9 block ENG
//
//   Collision2D::Surface        : Object::External  0x30  9 slots (Compile inherited)
//   Collision2D::CollisionMesh  : Object::External  0x70  9 slots (Compile overridden)
//
// Both were file-local PODs built by the FACTREST step table until block ENG
// defined the classes (B39_RULES D1/D2, rule B9-16): concrete, protected
// non-virtual dtor, slot order by PMF, the real ctor's retail stores, both
// DeletingDtor arms. The out-of-line members are MARKERS here
// (runtime/src/collision2d does not link into a host test); the ctors and
// DeletingDtor are inline in the headers and are the code under test.
// ============================================================================
#include "unit.h"

#include "runtime/types/collision2d/Surface.h"
#include "runtime/types/collision2d/CollisionMesh.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace {
int         g_dtorCount = 0;
const void* g_dtorOrder[8] = {};
void NoteDtor(const void* self) {
    if (g_dtorCount < 8) g_dtorOrder[g_dtorCount] = self;
    ++g_dtorCount;
}
}  // namespace

namespace Collision2D {
int32_t Surface::GetFileVersion() const { return 1; }
bool Surface::Serialize(void*) { return true; }
Object::External* Surface::Reconstruct() { return this; }
void Surface::Destruct() {}
Object::External* Surface::Construct() { return this; }
Surface::~Surface() { NoteDtor(this); }

int32_t CollisionMesh::GetFileVersion() const { return 3; }
Object::External::CompileResult CollisionMesh::Compile(const System::Filename&) { return 1; }
bool CollisionMesh::Serialize(void*) { return true; }
Object::External* CollisionMesh::Reconstruct() { return this; }
void CollisionMesh::Destruct() {}
Object::External* CollisionMesh::Construct() { return this; }
CollisionMesh::~CollisionMesh() { NoteDtor(this); }
}  // namespace Collision2D

namespace {

template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);
    if (words[1] != 0) return ~uintptr_t(0);
    return words[0] - 1;
}
const void* VTableOf(const void* obj) { return *reinterpret_cast<const void* const*>(obj); }

// Array-arm driver: `n` elements behind an 8-byte cookie, destroyed through
// element 0's slot 0 with flags 3.
template <class T, int kSize>
void CheckBothArms() {
    void* m = Object::detail::InternalMemAlloc(kSize, 8);
    CHECK(m != nullptr);
    T* p = ::new (m) T();
    g_dtorCount = 0;
    CHECK(p->DeletingDtor(1) == p);
    CHECK_EQ(g_dtorCount, 1);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * kSize, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    T* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * kSize) T();
    g_dtorCount = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_dtorCount, 3);
    CHECK(g_dtorOrder[0] == e[2]);
    CHECK(g_dtorOrder[1] == e[1]);
    CHECK(g_dtorOrder[2] == e[0]);
}

}  // namespace

UNIT_TEST(b39_eng_collision2d_classes_are_concrete) {
    CHECK((std::is_base_of<Object::External, Collision2D::Surface>::value));
    CHECK((std::is_base_of<Object::External, Collision2D::CollisionMesh>::value));
    CHECK(!std::is_abstract<Collision2D::Surface>::value);
    CHECK(!std::is_abstract<Collision2D::CollisionMesh>::value);
    CHECK(!std::is_destructible<Collision2D::Surface>::value);
    CHECK(!std::is_destructible<Collision2D::CollisionMesh>::value);
    CHECK(!std::has_virtual_destructor<Collision2D::Surface>::value);
    CHECK(!std::has_virtual_destructor<Collision2D::CollisionMesh>::value);
    CHECK_EQ((int)sizeof(Collision2D::Surface), 0x30);
    CHECK_EQ((int)sizeof(Collision2D::CollisionMesh), 0x70);
}

UNIT_TEST(b39_eng_surface_slot_order_by_pmf) {
    using S = Collision2D::Surface;
    CHECK_EQ(SlotOffset(&S::DeletingDtor),   (uintptr_t)0x00);   // 0x180064ea0
    CHECK_EQ(SlotOffset(&S::TypeName),       (uintptr_t)0x08);   // 0x180064a70
    CHECK_EQ(SlotOffset(&S::Type),           (uintptr_t)0x10);   // 0x180064a50
    CHECK_EQ(SlotOffset(&S::GetFileVersion), (uintptr_t)0x18);   // 0x18010ca30
    CHECK_EQ(SlotOffset(&S::Compile),        (uintptr_t)0x20);   // External's 0x1800066e0
    CHECK_EQ(SlotOffset(&S::Serialize),      (uintptr_t)0x28);   // 0x18010ca40
    CHECK_EQ(SlotOffset(&S::Reconstruct),    (uintptr_t)0x30);   // 0x180064c70
    CHECK_EQ(SlotOffset(&S::Destruct),       (uintptr_t)0x38);   // 0x180064d40
    CHECK_EQ(SlotOffset(&S::Construct),      (uintptr_t)0x40);   // 0x180064ce0
    // slot 4 is NOT overridden: it is Object::External's own Compile.
    CHECK((std::is_same<decltype(&S::Compile),
                        Object::External::CompileResult (Object::External::*)(const System::Filename&)>::value));
}

UNIT_TEST(b39_eng_collisionmesh_slot_order_by_pmf) {
    using C = Collision2D::CollisionMesh;
    CHECK_EQ(SlotOffset(&C::DeletingDtor),   (uintptr_t)0x00);   // 0x1800656b0
    CHECK_EQ(SlotOffset(&C::TypeName),       (uintptr_t)0x08);   // 0x180064f70
    CHECK_EQ(SlotOffset(&C::Type),           (uintptr_t)0x10);   // 0x180064f50
    CHECK_EQ(SlotOffset(&C::GetFileVersion), (uintptr_t)0x18);   // 0x180108cc0
    CHECK_EQ(SlotOffset(&C::Compile),        (uintptr_t)0x20);   // 0x180108cb0
    CHECK_EQ(SlotOffset(&C::Serialize),      (uintptr_t)0x28);   // 0x180108cd0
    CHECK_EQ(SlotOffset(&C::Reconstruct),    (uintptr_t)0x30);   // 0x1800651b0
    CHECK_EQ(SlotOffset(&C::Destruct),       (uintptr_t)0x38);   // 0x1800652c0
    CHECK_EQ(SlotOffset(&C::Construct),      (uintptr_t)0x40);   // 0x180065240
    CHECK((std::is_same<decltype(&C::Compile),
                        Object::External::CompileResult (C::*)(const System::Filename&)>::value));
}

// Surface's factory stores (0x180064bc3..bf4): qword +0x20, dword +0x28;
// +0x2c is never written.
UNIT_TEST(b39_eng_surface_ctor_makes_retails_stores) {
    alignas(8) uint8_t buf[0x30];
    std::memset(buf, 0xCD, sizeof buf);
    Collision2D::Surface* s = ::new (buf) Collision2D::Surface();
    CHECK(static_cast<void*>(s) == static_cast<void*>(buf));
    for (int off = 0x08; off < 0x2c; ++off) CHECK_EQ((int)buf[off], 0);
    for (int off = 0x2c; off < 0x30; ++off) CHECK_EQ((int)buf[off], 0xCD);
    alignas(8) uint8_t buf2[0x30];
    Collision2D::Surface* s2 = ::new (buf2) Collision2D::Surface();
    CHECK(VTableOf(s) != nullptr);
    CHECK(VTableOf(s) == VTableOf(s2));
    g_dtorCount = 0;
    CHECK(s->DeletingDtor(0) == s);
    CHECK(s2->DeletingDtor(0) == s2);
    CHECK_EQ(g_dtorCount, 2);
}

// CollisionMesh's factory stores (0x1800650c3..114): ten zero qwords
// +0x20..+0x68 after External's three.
UNIT_TEST(b39_eng_collisionmesh_ctor_makes_retails_stores) {
    alignas(8) uint8_t buf[0x70];
    std::memset(buf, 0xCD, sizeof buf);
    Collision2D::CollisionMesh* c = ::new (buf) Collision2D::CollisionMesh();
    CHECK(static_cast<void*>(c) == static_cast<void*>(buf));
    for (int off = 0x08; off < 0x70; off += 8) {
        uint64_t q = 1;
        std::memcpy(&q, buf + off, sizeof q);
        CHECK_EQ(q, (uint64_t)0);
    }
    g_dtorCount = 0;
    CHECK(c->DeletingDtor(0) == c);
    CHECK_EQ(g_dtorCount, 1);
}

UNIT_TEST(b39_eng_surface_deletingdtor_frees_both_arms) {
    CheckBothArms<Collision2D::Surface, 0x30>();
}
UNIT_TEST(b39_eng_collisionmesh_deletingdtor_frees_both_arms) {
    CheckBothArms<Collision2D::CollisionMesh, 0x70>();
}
