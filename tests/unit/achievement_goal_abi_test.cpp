// ============================================================================
// tests/unit/achievement_goal_abi_test.cpp — ENDGAME B.3.7 block B3
// The ABI claims `Achievement::{Goal, GoalInstance, GoalList, Achievement,
// AchievementInstance, AchievementManager}` make (B37_RULES.md B7-1, B7-4,
// B7-5, B7-8), measured on the SHIPPED HEADERS.
// ============================================================================
//
// PART A — LAYOUT. Every size, alignment and field offset against the retail
//   instrument that pinned it (B37_SLOTS.md §1, §7).
//
// PART B — SLOT ORDER OF THE REAL CLASSES, with no link dependency. A pointer
//   to a VIRTUAL member function is, in the Itanium C++ ABI GCC uses on this
//   target, `{ 1 + vtable byte offset, this-adjustment }`. So `&Goal::Serialize`
//   READS the slot the header gives `Serialize` without instantiating, calling
//   or linking anything — this is the header under test, not a copy of it.
//   (graphics_geometry_abi_test.cpp used a mirrored probe for this; the PMF
//   reading is stronger because it cannot drift from the header.)
//
// PART C — RULE B7-4, the one retslot VIRTUAL: `Goal::GetInstanceType`
//   (slot 9, 0x18009eeb0) returns `Object::Type` BY VALUE. MSVC passes `this`
//   in RCX and the caller's storage in RDX; GCC's ms_abi puts the hidden slot
//   FIRST. PART C shows the swap on a throwaway class, then shows that the
//   declared shape — the out-parameter spelled in retail order — receives a
//   raw retail-order call (`(this, ret)`, result in RAX) correctly, which is
//   what GoalList::Initialize's `CALL [RAX+0x48]` and the exe's TimeGoal /
//   FinishTutorialGoal slot-9 bodies do.
//
// PART D — RULE B7-5, hidden-pointer ARGUMENTS: `GoalList::AreGoalsMetExcept`
//   (InternalPtr by value, `MOV RDX,[RDX]`) and `GoalList::Initialize` (Array
//   by value, `MOV EDX,[RDX+0x8]`) receive a POINTER in RDX. Their declared
//   types are pinned, and one raw retail-order call per argument type is made
//   into a probe with the same signature shape.
//
// WHAT IS *NOT* PROVEN HERE: the bodies. runtime/src/achievement/ stands on
// the port registry and the allocator and cannot link into a host test; the
// bodies are verified by the real run.
// ============================================================================
#include "unit.h"

#include "runtime/types/achievement/Goal.h"
#include "runtime/types/achievement/AchievementManager.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

using Achievement::Goal;
using Achievement::GoalInstance;
using Achievement::GoalList;
using AchClass = Achievement::Achievement;
using Achievement::AchievementInstance;
using Achievement::AchievementManager;

// The vtable byte offset a pointer-to-VIRTUAL-member names (Itanium ABI:
// ptr = 1 + offset, adj = 0 for single inheritance). Returns ~0 for a
// non-virtual member, so a test that expects a slot fails loudly on one.
template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting: not single inheritance
    return words[0] - 1;
}

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================

UNIT_TEST(achievement_sizes_match_retail) {
    // Each polymorphic size is the `MOV EDX,imm` in the class's own slot-0
    // deleting destructor (B37_SLOTS §9); the others are the allocation or the
    // ctor's last store (B37_RULES §2.3).
    CHECK_EQ(sizeof(Goal),                (size_t)0x30);   // slot 0 0x180001d70
    CHECK_EQ(sizeof(GoalInstance),        (size_t)0x20);   // slot 0 0x1800020c0
    CHECK_EQ(sizeof(GoalList),            (size_t)0x10);   // ctor 0x18009f0b0
    CHECK_EQ(sizeof(AchClass),            (size_t)0x58);   // slot 0 0x180001680
    CHECK_EQ(sizeof(AchievementInstance), (size_t)0x28);   // slot 0 0x180001a90
    CHECK_EQ(sizeof(AchievementManager),  (size_t)0x48);   // MemAlloc(0x48) 0x1800dbe42
    CHECK_EQ(sizeof(Object::Type),        (size_t)4);      // GetInstanceType's dword store

    CHECK_EQ(alignof(Goal),                (size_t)8);
    CHECK_EQ(alignof(GoalInstance),        (size_t)8);
    CHECK_EQ(alignof(GoalList),            (size_t)8);
    CHECK_EQ(alignof(AchClass),            (size_t)8);
    CHECK_EQ(alignof(AchievementInstance), (size_t)8);
    CHECK_EQ(alignof(AchievementManager),  (size_t)8);
}

UNIT_TEST(achievement_field_offsets) {
    CHECK_EQ(offsetof(Goal, _stringTable), (size_t)0x20);        // GetStringTable 0x180001b90
    CHECK_EQ(offsetof(Goal, _text),        (size_t)0x28);        // GetText 0x180001ba0
    CHECK_EQ(offsetof(GoalInstance, _flags), (size_t)0x10);      // IsMet 0x18009eed0
    CHECK_EQ(offsetof(GoalInstance, _goal),  (size_t)0x18);      // GetGoal 0x180001fe0
    CHECK_EQ(offsetof(GoalList, _goals),     (size_t)0x00);
    CHECK_EQ(offsetof(AchClass, _stringTable), (size_t)0x20);
    CHECK_EQ(offsetof(AchClass, _title),       (size_t)0x28);
    CHECK_EQ(offsetof(AchClass, _description), (size_t)0x30);
    CHECK_EQ(offsetof(AchClass, _spriteSheet), (size_t)0x38);
    CHECK_EQ(offsetof(AchClass, _spriteName),  (size_t)0x40);
    CHECK_EQ(offsetof(AchClass, _goals),       (size_t)0x48);
    CHECK_EQ(offsetof(AchievementInstance, _goalList),    (size_t)0x10);
    CHECK_EQ(offsetof(AchievementInstance, _achievement), (size_t)0x20);
    CHECK_EQ(offsetof(AchievementManager, _completed),             (size_t)0x08);
    CHECK_EQ(offsetof(AchievementManager, _instances),             (size_t)0x18);
    CHECK_EQ(offsetof(AchievementManager, _eventHandler),          (size_t)0x28);
    CHECK_EQ(offsetof(AchievementManager, _updateTimes),           (size_t)0x30);
    CHECK_EQ(offsetof(AchievementManager, _lastAchievementUpdate), (size_t)0x40);
}

UNIT_TEST(achievement_class_shape_rules) {
    // B7-13 / ENDGAME B.1 decision 2: polymorphic, abstract, NO virtual dtor.
    CHECK(std::is_polymorphic<Goal>::value);
    CHECK(std::is_abstract<Goal>::value);
    CHECK(std::is_abstract<GoalInstance>::value);
    CHECK(std::is_abstract<AchClass>::value);
    CHECK(!std::is_abstract<AchievementInstance>::value);   // concrete since B.3.8 RB (exe table copy 0x14015e350)
    CHECK(!std::has_virtual_destructor<Goal>::value);
    CHECK(!std::has_virtual_destructor<GoalInstance>::value);
    CHECK(!std::has_virtual_destructor<AchClass>::value);
    CHECK(!std::has_virtual_destructor<AchievementInstance>::value);
    // B7-8: GoalList is a value type with a DEEP copy and an owning dtor.
    CHECK(!std::is_polymorphic<GoalList>::value);
    CHECK(!std::is_trivially_copyable<GoalList>::value);
    CHECK(!std::is_trivially_destructible<GoalList>::value);
    CHECK(!std::is_polymorphic<AchievementManager>::value);
    // The bases retail derives from, at offset 0 (no multiple inheritance).
    CHECK((std::is_base_of<Object::External, Goal>::value));
    CHECK((std::is_base_of<Object::Internal, GoalInstance>::value));
    CHECK((std::is_base_of<Object::External, AchClass>::value));
    CHECK((std::is_base_of<Object::Internal, AchievementInstance>::value));
    // Object::Type is a value written into a raw hidden slot.
    CHECK(std::is_trivially_copyable<Object::Type>::value);
}

// ===========================================================================
// PART B — SLOT ORDER OF THE REAL CLASSES (B37_SLOTS.md §1)
// ===========================================================================

UNIT_TEST(achievement_goal_slots_are_retail_order) {
    // ??_7Goal@Achievement@@6B@ 0x1801546b0, ten slots.
    CHECK_EQ(SlotOffset(&Goal::DeletingDtor),    (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&Goal::TypeName),        (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&Goal::Type),            (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&Goal::GetFileVersion),  (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&Goal::Compile),         (uintptr_t)0x20);   // External's, inherited
    CHECK_EQ(SlotOffset(&Goal::Serialize),       (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&Goal::Reconstruct),     (uintptr_t)0x30);   // pure, inherited
    CHECK_EQ(SlotOffset(&Goal::Destruct),        (uintptr_t)0x38);   // pure, inherited
    CHECK_EQ(SlotOffset(&Goal::CheckGoalState),  (uintptr_t)0x40);   // pure, Goal's own
    CHECK_EQ(SlotOffset(&Goal::GetInstanceType), (uintptr_t)0x48);   // Goal.h's old kGetInstanceType
}

UNIT_TEST(achievement_goalinstance_slots_are_retail_order) {
    // ??_7GoalInstance@Achievement@@6B@ 0x180154678, seven slots (B7-1.8).
    CHECK_EQ(SlotOffset(&GoalInstance::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&GoalInstance::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&GoalInstance::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&GoalInstance::Persist),        (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&GoalInstance::Startup),        (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&GoalInstance::Update),         (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&GoalInstance::CheckGoalState), (uintptr_t)0x30);
}

UNIT_TEST(achievement_achievement_slots_are_retail_order) {
    // ??_7Achievement@0@6B@ 0x180154740, nine slots.
    CHECK_EQ(SlotOffset(&AchClass::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&AchClass::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&AchClass::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&AchClass::GetFileVersion), (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&AchClass::Compile),        (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&AchClass::Serialize),      (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&AchClass::Reconstruct),    (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&AchClass::Destruct),       (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&AchClass::Construct),      (uintptr_t)0x40);
    // ??_7AchievementInstance@Achievement@@6B@ 0x180154700, four slots.
    CHECK_EQ(SlotOffset(&AchievementInstance::DeletingDtor), (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&AchievementInstance::TypeName),     (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&AchievementInstance::Type),         (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&AchievementInstance::Persist),      (uintptr_t)0x18);
}

// ===========================================================================
// PART C — RULE B7-4: slot 9 carries a HIDDEN RETURN SLOT, in retail order
// ===========================================================================

// The declared type IS the retail-order shape — `(this, ret)`, result `ret`.
static_assert(std::is_same<decltype(&Goal::GetInstanceType),
                           Object::Type* (Goal::*)(Object::Type*) const>::value,
              "B7-4: GetInstanceType spells the hidden return slot explicitly");

// A throwaway class whose slot 0 returns an 8-byte-plus class BY VALUE the
// C++ way, to show GCC's ms_abi order (storage FIRST, then `this`).
struct AbiBig { uint32_t v[5]; };
class AbiByValue {
public:
    virtual AbiBig Get(int n) const { AbiBig r{}; r.v[0] = static_cast<uint32_t>(tag + n); return r; }
    int tag = 0;
protected:
    ~AbiByValue() = default;
};
class AbiByValueLeaf final : public AbiByValue {};

// A probe with the SAME slot-9 declaration shape as Achievement::Goal. It
// mirrors (not derives from) Goal: Goal's bodies cannot link into a host test.
class ProbeGoal {
public:
    virtual void* Slot0(unsigned) { return this; }
    virtual const char* Slot1() const { return nullptr; }
    virtual uint32_t Slot2() const { return 0; }
    virtual int32_t Slot3() const { return 0; }
    virtual uint32_t Slot4(const void*) { return 1; }
    virtual bool Slot5(void*) { return true; }
    virtual void* Slot6() { return nullptr; }
    virtual void Slot7() {}
    virtual int32_t Slot8() const { return 0; }
    virtual Object::Type* GetInstanceType(Object::Type* ret) const {   // [9]
        ret->_type = typeId;
        return ret;
    }
    uint32_t typeId = 0;
protected:
    ~ProbeGoal() = default;
};
class ProbeGoalLeaf final : public ProbeGoal {};

UNIT_TEST(achievement_gcc_sret_precedes_this_unlike_msvc) {
    using GccOrder = AbiBig* (*)(AbiBig* ret, const void* self, int n);
    AbiByValueLeaf p; p.tag = 40;
    void* slot0 = (*reinterpret_cast<void***>(&p))[0];
    AbiBig out{};
    AbiBig* got = reinterpret_cast<GccOrder>(slot0)(&out, &p, 2);
    CHECK_EQ(got, &out);          // GCC wants the storage FIRST...
    CHECK_EQ(out.v[0], 42u);      // ...so a retail-order `(this, ret)` caller would
    CHECK_EQ(p.tag, 40);          // hand it `this` as the storage: a wild write.
}

UNIT_TEST(achievement_goal_slot9_takes_this_then_retslot) {
    ProbeGoalLeaf g; g.typeId = 0x1234abcdu;
    // GoalList::Initialize (0x18009f13f): RCX = the goal, RDX = &slot, CALL [RAX+0x48].
    using RetailOrder = Object::Type* (*)(const void* self, Object::Type* ret);
    void* slot9 = (*reinterpret_cast<void***>(&g))[0x48 / 8];
    Object::Type slot; slot._type = 0xdeadbeefu;
    Object::Type* got = reinterpret_cast<RetailOrder>(slot9)(&g, &slot);
    CHECK_EQ(got, &slot);                   // RAX == the caller's storage
    CHECK_EQ(slot._type, 0x1234abcdu);      // and the id is in it
    CHECK_EQ(g.typeId, 0x1234abcdu);        // `this` untouched
    CHECK_EQ(static_cast<uint32_t>(*got), 0x1234abcdu);   // ??BType — operator unsigned int
}

// ===========================================================================
// PART D — RULE B7-5: hidden-pointer ARGUMENTS, and the bare-register ones
// ===========================================================================

static_assert(std::is_same<decltype(&GoalList::AreGoalsMetExcept),
                           bool (GoalList::*)(const Object::InternalPtr<GoalInstance>*) const>::value,
              "B7-5: InternalPtr by value arrives by HIDDEN POINTER (MOV RDX,[RDX])");
static_assert(std::is_same<decltype(&GoalList::Initialize),
                           void (GoalList::*)(Achievement::GoalArray*)>::value,
              "B7-5: Array by value arrives by HIDDEN POINTER (MOV EDX,[RDX+0x8])");
static_assert(std::is_same<decltype(&AchievementInstance::SetAchievement),
                           void (AchievementInstance::*)(Object::ExternalPtr<AchClass>)>::value,
              "B7-5: ExternalPtr by value is a BARE register (MOV [RCX+0x20],RDX)");
static_assert(std::is_same<decltype(&AchievementManager::SetAchievements),
                           void (AchievementManager::*)(Object::ExternalPtr<Object::ExternalList>)>::value,
              "B7-5: ExternalPtr by value is a BARE register (MOV [RCX],RDX)");
static_assert(std::is_trivially_copyable<Object::ExternalPtr<AchClass>>::value &&
              sizeof(Object::ExternalPtr<AchClass>) == 8,
              "an 8-byte trivially copyable class travels in one register");

// Probe members with the same parameter shapes, called raw in retail order.
struct ProbeList {
    int seen = 0;
    bool AreGoalsMetExcept(const Object::InternalPtr<GoalInstance>* except) const {
        return except->Get() == reinterpret_cast<GoalInstance*>(uintptr_t(0x5000));
    }
    void Initialize(Achievement::GoalArray* goals) { seen = goals->GetSize(); }
};

bool ProbeAreMet(const ProbeList* self, const Object::InternalPtr<GoalInstance>* e) {
    return self->AreGoalsMetExcept(e);
}
void ProbeInit(ProbeList* self, Achievement::GoalArray* a) { self->Initialize(a); }

UNIT_TEST(achievement_hidden_pointer_arguments_retail_order) {
    ProbeList list;
    // `MOV RDX,[RDX]` — RDX holds the ADDRESS of the caller's InternalPtr copy.
    Object::InternalPtr<GoalInstance> except(reinterpret_cast<GoalInstance*>(uintptr_t(0x5000)));
    using AreMetRaw = bool (*)(const void* self, const void* exceptCopy);
    CHECK(reinterpret_cast<AreMetRaw>(&ProbeAreMet)(&list, &except));

    // `MOV EDX,[RDX+0x8]` — RDX holds the ADDRESS of the caller's Array temp.
    Achievement::GoalArray temp;
    temp._size = 7;   // the count field at +0x08 is what retail reads first
    using InitRaw = void (*)(void* self, void* arrayTemp);
    reinterpret_cast<InitRaw>(&ProbeInit)(&list, &temp);
    CHECK_EQ(list.seen, 7);
    temp._size = 0;   // nothing was allocated: leave the header empty for ~Array
}

}  // namespace
