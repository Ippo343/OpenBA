// ============================================================================
// tests/unit/pathfinding_picking_abi_test.cpp — ENDGAME B.3 step 7, block B6
//
//   Pathfinding::PointList  : Object::External  0x50  9 slots, Compile OVERRIDDEN
//   Pathfinding::IPathInfo  : (root)            0x08  2 pure slots, no dtor slot
//   Pathfinding::Scene      : non-polymorphic   0x90  declarations only
//
// Built to the contract of ui_resources_abi_test.cpp (step 6, block D); only
// what differs is spelled out.
//
// PART A — LAYOUT: sizeof / alignof / every offset the headers claim, the
//   parent edge `_ParentTypeName` proves, abstract, no public default ctor.
// PART B — SLOT ORDER: a probe subclass of the REAL class; each own slot is
//   raw-called by byte offset and the marker that runs names the member.
//   Slot 4 `Compile` is the class's OWN body (B7-1.7), a different pointer from
//   Object::External's, answering retail's 1 in EAX (no hidden slot).
// PART C — THE QUALIFIED DOOR CALL: a minted-table slot pointing at a door that
//   delegates `->Ns::C::M()` enters the door once and lands in the member.
//
// The classes' out-of-line members are defined HERE as markers
// (runtime/src/pathfinding and runtime/src/picking do not link into a host test).
// ============================================================================
#include "unit.h"

#include "runtime/types/pathfinding/PointList.h"
#include "runtime/types/pathfinding/IPathInfo.h"
#include "runtime/types/pathfinding/Scene.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

const void* g_lastSelf = nullptr;
int         g_lastMark = -1;
void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; }

template <typename Fn>
Fn SlotAt(void* obj, int byteOffset) {
    const uint8_t* vt = *reinterpret_cast<const uint8_t* const*>(obj);
    Fn f = nullptr;
    const void* raw = *reinterpret_cast<const void* const*>(vt + byteOffset);
    std::memcpy(&f, &raw, sizeof f);
    return f;
}
const void* const* VTableOf(void* obj) {
    return *reinterpret_cast<const void* const* const*>(obj);
}

constexpr int kDeletingDtor   = 0x00;
constexpr int kTypeName       = 0x08;
constexpr int kType           = 0x10;
constexpr int kGetFileVersion = 0x18;
constexpr int kCompile        = 0x20;
constexpr int kSerialize      = 0x28;
constexpr int kReconstruct    = 0x30;
constexpr int kDestruct       = 0x38;
constexpr int kConstruct      = 0x40;   // introduced per leaf — B7-1.6

// A bare Object::External leaf: whatever it carries at +0x20 IS
// `?Compile@External@Object@@`.
struct BareExternalProbe final : Object::External {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};

// Raw-call slots 0..8 (4 included: Compile is overridden on both resources
// here) and check the marker each one leaves.
void CheckNineOwnSlots(void* raw) {
    using DtorFn    = void* (*)(void*, unsigned);
    using NameFn    = const char* (*)(const void*);
    using TypeFn    = uint32_t (*)(const void*);
    using VerFn     = int32_t (*)(const void*);
    using CompileFn = uint32_t (*)(void*, const void*);
    using SerFn     = bool (*)(void*, void*);
    using PtrFn     = void* (*)(void*);
    using VoidFn    = void (*)(void*);

    Reset(); SlotAt<DtorFn>(raw, kDeletingDtor)(raw, 0u);
    CHECK_EQ(g_lastMark, kDeletingDtor / 8); CHECK(g_lastSelf == raw);
    Reset(); SlotAt<NameFn>(raw, kTypeName)(raw);       CHECK_EQ(g_lastMark, kTypeName / 8);
    Reset(); SlotAt<TypeFn>(raw, kType)(raw);           CHECK_EQ(g_lastMark, kType / 8);
    Reset(); SlotAt<VerFn>(raw, kGetFileVersion)(raw);  CHECK_EQ(g_lastMark, kGetFileVersion / 8);
    Reset();
    const uint32_t r = SlotAt<CompileFn>(raw, kCompile)(raw, nullptr);
    CHECK_EQ(g_lastMark, kCompile / 8);
    CHECK_EQ(r, 1u);                                    // retail's `MOV EAX,1`
    Reset(); SlotAt<SerFn>(raw, kSerialize)(raw, nullptr); CHECK_EQ(g_lastMark, kSerialize / 8);
    Reset(); SlotAt<PtrFn>(raw, kReconstruct)(raw);     CHECK_EQ(g_lastMark, kReconstruct / 8);
    Reset(); SlotAt<VoidFn>(raw, kDestruct)(raw);       CHECK_EQ(g_lastMark, kDestruct / 8);
    Reset(); SlotAt<PtrFn>(raw, kConstruct)(raw);       CHECK_EQ(g_lastMark, kConstruct / 8);
    CHECK(g_lastSelf == raw);

    const void* const* vt = VTableOf(raw);
    for (int i = 0; i < 9; ++i)
        for (int j = i + 1; j < 9; ++j)
            CHECK(vt[i] != vt[j]);

    BareExternalProbe bare;
    CHECK(vt[kCompile / 8] != VTableOf(static_cast<void*>(&bare))[kCompile / 8]);
}

}  // namespace

// ===========================================================================
// Pathfinding::PointList's out-of-line members, AS MARKERS.
// ===========================================================================
namespace Pathfinding {
const char* PointList::TypeName() const { Note(this, 1); return "PointList"; }
uint32_t PointList::Type() const { Note(this, 2); return 0x9151u; }
int32_t PointList::GetFileVersion() const { Note(this, 3); return 5; }
Object::External::CompileResult PointList::Compile(const System::Filename&) { Note(this, 4); return 1; }
bool PointList::Serialize(void*) { Note(this, 5); return true; }
Object::External* PointList::Reconstruct() { Note(this, 6); return this; }
void PointList::Destruct() { Note(this, 7); }
Object::External* PointList::Construct() { Note(this, 8); return this; }
PointList::~PointList() {}
}  // namespace Pathfinding

namespace {

struct PointListProbe final : Pathfinding::PointList {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};

// Slot 3 of a minted PointList table points at the DOOR, and the door delegates
// with the qualified call (pointlist.cpp's RtPathfinding_PointList_GetFileVersion).
int g_doorEntries = 0;
int32_t MintedPointListGetFileVersionDoor(const void* self) {
    ++g_doorEntries;
    return reinterpret_cast<const Pathfinding::PointList*>(self)      // B.3.7 bridge
               ->Pathfinding::PointList::GetFileVersion();
}

}  // namespace

// ---------------------------------------------------------------------------
// PART A — Pathfinding::PointList layout
// ---------------------------------------------------------------------------
UNIT_TEST(pathfinding_pointlist_layout) {
    CHECK_EQ((int)sizeof(Pathfinding::PointList), 0x50);
    CHECK_EQ((int)alignof(Pathfinding::PointList), 8);
    CHECK_EQ((int)offsetof(Pathfinding::PointList, _meshName),   0x20);
    CHECK_EQ((int)offsetof(Pathfinding::PointList, _string28),   0x28);
    CHECK_EQ((int)offsetof(Pathfinding::PointList, _points),     0x30);
    CHECK_EQ((int)offsetof(Pathfinding::PointList, _pathPoints), 0x40);
    CHECK_EQ((int)offsetof(Pathfinding::PointList, _meshName), (int)sizeof(Object::External));
    CHECK_EQ((int)sizeof(Pathfinding::PointList::Point), 0x18);
    CHECK_EQ((int)offsetof(Pathfinding::PointList::Point, pos),    0x08);
    CHECK_EQ((int)offsetof(Pathfinding::PointList::Point, radius), 0x14);
    // interactdescription.cpp walks `_points` at +0x30 (data) / +0x38 (size).
    CHECK_EQ((int)(offsetof(Pathfinding::PointList, _points) +
                   offsetof(Collection::Array<Pathfinding::PointList::Point>, _size)), 0x38);
    CHECK((std::is_base_of<Object::External, Pathfinding::PointList>::value));
    CHECK(std::is_abstract<Pathfinding::PointList>::value);
    CHECK(!std::is_default_constructible<Pathfinding::PointList>::value);
}

// The two reference getters return the members retail's LEAs name
// (0x180071190 +0x30, 0x1800711a0 +0x40): no hidden return slot.
UNIT_TEST(pathfinding_pointlist_getters_read_retails_offsets) {
    PointListProbe probe;
    const Pathfinding::PointList& pl = probe;
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&pl);
    CHECK(reinterpret_cast<const uint8_t*>(&pl.Points())     == base + 0x30);
    CHECK(reinterpret_cast<const uint8_t*>(&pl.PathPoints()) == base + 0x40);
}

// ---------------------------------------------------------------------------
// PART B — slot order
// ---------------------------------------------------------------------------
UNIT_TEST(pathfinding_pointlist_slot_order_is_retails) {
    PointListProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckNineOwnSlots(raw);
    CHECK(static_cast<void*>(static_cast<Object::External*>(&probe)) == raw);
}

// ---------------------------------------------------------------------------
// PART C — the qualified door call does not re-enter the door
// ---------------------------------------------------------------------------
UNIT_TEST(pathfinding_pointlist_qualified_call_does_not_reenter_the_door) {
    PointListProbe probe;
    void* raw = static_cast<void*>(&probe);
    const void* minted[9] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 9; ++i) minted[i] = real[i];
    minted[kGetFileVersion / 8] = reinterpret_cast<const void*>(&MintedPointListGetFileVersionDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_doorEntries = 0;
    Reset();
    using VerFn = int32_t (*)(const void*);
    const int32_t v = SlotAt<VerFn>(raw, kGetFileVersion)(raw);
    CHECK_EQ(g_doorEntries, 1);
    CHECK_EQ(g_lastMark, kGetFileVersion / 8);
    CHECK_EQ(v, 5);                      // retail's literal (0x18010f880)

    *reinterpret_cast<const void* const**>(raw) = real;
}

// ---------------------------------------------------------------------------
// Pathfinding::IPathInfo and Pathfinding::Scene — dead in retail, declarations
// only; their layouts are still pinned.
// ---------------------------------------------------------------------------
namespace {
struct PathInfoProbe final : Pathfinding::IPathInfo {
    void Slot0() override { Note(this, 100); }
    void Slot1() override { Note(this, 101); }
};
}  // namespace

UNIT_TEST(pathfinding_ipathinfo_is_two_pure_slots_and_no_dtor_slot) {
    CHECK_EQ((int)sizeof(Pathfinding::IPathInfo), 8);
    CHECK(std::is_abstract<Pathfinding::IPathInfo>::value);
    CHECK(!std::has_virtual_destructor<Pathfinding::IPathInfo>::value);
    PathInfoProbe probe;
    void* raw = static_cast<void*>(&probe);
    using VoidFn = void (*)(void*);
    Reset(); SlotAt<VoidFn>(raw, 0x00)(raw); CHECK_EQ(g_lastMark, 100);
    Reset(); SlotAt<VoidFn>(raw, 0x08)(raw); CHECK_EQ(g_lastMark, 101);
}

UNIT_TEST(pathfinding_scene_layout) {
    CHECK_EQ((int)sizeof(Pathfinding::Scene), 0x90);
    CHECK_EQ((int)alignof(Pathfinding::Scene), 8);
    CHECK(!std::is_polymorphic<Pathfinding::Scene>::value);
    CHECK_EQ((int)offsetof(Pathfinding::Scene, _nodes),   0x08);
    CHECK_EQ((int)offsetof(Pathfinding::Scene, _links),   0x68);
    CHECK_EQ((int)offsetof(Pathfinding::Scene, _array78), 0x78);
    CHECK_EQ((int)offsetof(Pathfinding::Scene, _int88),   0x88);
    CHECK_EQ((int)offsetof(Pathfinding::Scene, _radius),  0x8c);
    // B7-8: the retail copy constructor is DEEP — never trivially copyable.
    CHECK(!std::is_trivially_copyable<Pathfinding::Scene>::value);
}
