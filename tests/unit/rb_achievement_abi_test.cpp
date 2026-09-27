// ============================================================================
// tests/unit/rb_achievement_abi_test.cpp — ENDGAME B.3.8 block RB, the four
// Achievement Runtime bases GOAL (Scenario, the Goal/GoalInstance leaves) and
// LOOP (the AchievementInstance GameUpdateLoop builds) sit on.
//
// B38_RULES D1 / RULE B8-1: each gets a REAL constructor whose body is ONE
// body with its ctor door (`InitFields`, inline in the header, reached by the
// door through `X::DoorBody::Init` after it stamped the MINTED tables). The
// destructors were already real and empty (the compiler's member/base chain
// IS retail's body) and the doors call them; nothing changes there.
//
// PART A — RETAIL'S BYTES: a test subclass built over a 0xCD-poisoned buffer
//   carries exactly retail's writes (0x18009e240, 0x18009ed50, 0x18009ef00,
//   0x18009e3b0) and leaves the bytes retail leaves (GoalInstance +0x14).
// PART B — (retired in ENDGAME B.3.9 ENG with the ??0 doors; it compared the
//   door shape against the ctor.)
// PART C — AchievementInstance is CONCRETE: slot 0 is the exe's scalar
//   deleting dtor 0x140019d90 (~AchievementInstance, MemFree on flags & 1).
//
// The virtuals and the out-of-line dtors / GoalList members below are
// MARKERS (runtime/src/achievement stands on the registry and does not link
// here); `InitFields` itself is the REAL body — it is inline.
// ============================================================================
#include "unit.h"

#include "runtime/types/achievement/AchievementManager.h"
#include "runtime/types/achievement/Goal.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace Object {
namespace detail {
extern InternalFactory* g_unitInternalFactory;
}  // namespace detail
}  // namespace Object

extern const void* const g_rbAiVtGroup[] __asm__("_ZTVN11Achievement19AchievementInstanceE");

namespace {
struct FactoryScope {
    ::Object::InternalFactory  factory;
    ::Object::InternalFactory* saved;
    FactoryScope() : factory(), saved(::Object::detail::g_unitInternalFactory) {
        ::Object::detail::g_unitInternalFactory = &factory;
    }
    ~FactoryScope() { ::Object::detail::g_unitInternalFactory = saved; }
};
int g_aiDtorRuns = 0;
// ENDGAME B.3.9 ENG: the Goal / GoalInstance destructor markers RECORD each
// call, so the real inline DeletingDtors can be seen running per element.
int         g_dtorCount = 0;
const void* g_dtorOrder[8] = {};
void NoteDtor(const void* self) {
    if (g_dtorCount < 8) g_dtorOrder[g_dtorCount] = self;
    ++g_dtorCount;
}
}  // namespace

// ===========================================================================
// Markers (see the banner).
// ===========================================================================
namespace Achievement {
const char* Achievement::TypeName() const { return "Achievement"; }
uint32_t    Achievement::Type() const { return 0; }
int32_t     Achievement::GetFileVersion() const { return 3; }
bool        Achievement::Serialize(void*) { return true; }
Object::External* Achievement::Reconstruct() { return this; }
void        Achievement::Destruct() {}
Object::External* Achievement::Construct() { return this; }
Achievement::~Achievement() { NoteDtor(this); }

const char* AchievementInstance::TypeName() const { return "AchievementInstance"; }
uint32_t    AchievementInstance::Type() const { return 0; }
void        AchievementInstance::Persist(void*) {}
AchievementInstance::~AchievementInstance() { ++g_aiDtorRuns; }

const char* Goal::TypeName() const { return "Goal"; }
uint32_t    Goal::Type() const { return 0; }
int32_t     Goal::GetFileVersion() const { return 0x10000; }
bool        Goal::Serialize(void*) { return true; }
Object::Type* Goal::GetInstanceType(Object::Type* ret) const { return ret; }
Goal::~Goal() { NoteDtor(this); }

const char* GoalInstance::TypeName() const { return "GoalInstance"; }
uint32_t    GoalInstance::Type() const { return 0; }
void        GoalInstance::Persist(void*) {}
void        GoalInstance::Startup() {}
void        GoalInstance::Update(const System::Time&, const GoalList&) {}
Goal::GoalState GoalInstance::CheckGoalState() const { return Goal::kNotMet; }
GoalInstance::~GoalInstance() { NoteDtor(this); }

GoalList::GoalList() {}
GoalList::~GoalList() {}
}  // namespace Achievement

namespace {

using namespace ::Achievement;

struct RbScenario final : ::Achievement::Achievement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct RbGoal final : Goal {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    GoalState CheckGoalState() const override { return kNotMet; }
};
struct RbGoalInstance final : GoalInstance {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};

template <class T> T* BuildIn(unsigned char* buf) {
    std::memset(buf, 0xCD, sizeof(T));
    return ::new (static_cast<void*>(buf)) T;
}
uint64_t Q(const void* p, size_t off) {
    uint64_t v; std::memcpy(&v, static_cast<const unsigned char*>(p) + off, 8); return v;
}
uint32_t D(const void* p, size_t off) {
    uint32_t v; std::memcpy(&v, static_cast<const unsigned char*>(p) + off, 4); return v;
}

// ===========================================================================
// PART A + B
// ===========================================================================
// ENDGAME B.3.9 ENG: the ??0 doors are retired; the ctors are the only bodies.
UNIT_TEST(rb_achievement_ctor_is_retail_0x18009e240) {
    alignas(8) unsigned char a[sizeof(RbScenario)];
    RbScenario* s = BuildIn<RbScenario>(a);
    for (size_t off = 0x08; off < 0x58; off += 8) CHECK_EQ(Q(s, off), (uint64_t)0);
    s->~RbScenario();
}

// ENDGAME B.3.9 ENG: the ??0Goal / ??0GoalInstance doors are retired (the
// ctors are the only bodies); the door-shape halves of these two tests went
// with them.
UNIT_TEST(rb_goal_ctor_is_retail_0x18009ed50) {
    alignas(8) unsigned char a[sizeof(RbGoal)];
    RbGoal* g = BuildIn<RbGoal>(a);
    for (size_t off = 0x08; off < 0x30; off += 8) CHECK_EQ(Q(g, off), (uint64_t)0);
    g->~RbGoal();
}

UNIT_TEST(rb_goalinstance_ctor_is_retail_0x18009ef00) {
    FactoryScope scope;
    alignas(8) unsigned char a[sizeof(RbGoalInstance)];
    RbGoalInstance* g = BuildIn<RbGoalInstance>(a);
    CHECK_EQ(D(g, 0x10), 0u);             // _flags (a DWORD store)
    CHECK_EQ(D(g, 0x14), 0xCDCDCDCDu);    // +0x14 NEVER written
    CHECK_EQ(Q(g, 0x18), (uint64_t)0);    // _goal
    g->~RbGoalInstance();
}

// ---------------------------------------------------------------------------
// ENDGAME B.3.9 ENG (B39_RULES D1, rule B9-16): Goal / GoalInstance's own
// slot 0 -- retail's VECTOR deleting destructors (element 0x30 / 0x20).
// ---------------------------------------------------------------------------
// A Goal leaf that does NOT override slot 0, so Goal::DeletingDtor runs.
struct GoalLeaf final : Goal {
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    GoalState CheckGoalState() const override { return kNotMet; }
};

template <class T, int kSize>
void CheckBothArms() {
    void* m = Object::detail::InternalMemAlloc(kSize, 8);
    CHECK(m != nullptr);
    T* p = ::new (m) T();
    g_dtorCount = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(1)) == m);
    CHECK_EQ(g_dtorCount, 1);
    unsigned char* block = static_cast<unsigned char*>(Object::detail::InternalMemAlloc(8 + 3 * kSize, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    T* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * kSize) T();
    g_dtorCount = 0;
    CHECK(reinterpret_cast<unsigned char*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_dtorCount, 3);
    CHECK(g_dtorOrder[0] == static_cast<const void*>(e[2]));
    CHECK(g_dtorOrder[1] == static_cast<const void*>(e[1]));
    CHECK(g_dtorOrder[2] == static_cast<const void*>(e[0]));
}

UNIT_TEST(b39_eng_goal_deletingdtor_frees_both_arms) {
    CHECK_EQ(sizeof(GoalLeaf), (size_t)0x30);
    CheckBothArms<GoalLeaf, 0x30>();
}

UNIT_TEST(b39_eng_goalinstance_deletingdtor_frees_both_arms) {
    FactoryScope scope;
    CheckBothArms<GoalInstance, 0x20>();
}

// GoalInstance is concrete now: two instances share the compiler's table.
UNIT_TEST(b39_eng_goalinstance_ctor_is_the_class_table) {
    FactoryScope scope;
    alignas(8) unsigned char a[0x20], b[0x20];
    GoalInstance* x = ::new (static_cast<void*>(a)) GoalInstance();
    GoalInstance* y = ::new (static_cast<void*>(b)) GoalInstance();
    RbGoalInstance probe;
    CHECK(*reinterpret_cast<const void* const*>(x) == *reinterpret_cast<const void* const*>(y));
    CHECK(*reinterpret_cast<const void* const*>(x) != *reinterpret_cast<const void* const*>(&probe));
    g_dtorCount = 0;
    x->DeletingDtor(0);
    y->DeletingDtor(0);
    CHECK_EQ(g_dtorCount, 2);
}

UNIT_TEST(rb_achievementinstance_ctor_is_retail_0x18009e3b0) {
    FactoryScope scope;
    alignas(8) unsigned char a[sizeof(AchievementInstance)];
    std::memset(a, 0xCD, sizeof a);
    AchievementInstance* ai = ::new (static_cast<void*>(a)) AchievementInstance();
    CHECK(*reinterpret_cast<const void* const*>(ai) == &g_rbAiVtGroup[2]);
    for (size_t off = 0x10; off < 0x28; off += 8) CHECK_EQ(Q(ai, off), (uint64_t)0);
    ai->DeletingDtor(0);   // destruct only (flags 0: no free of a stack buffer)
}

UNIT_TEST(rb_achievementinstance_slot0_is_the_exe_scalar_deleting_dtor) {
    FactoryScope scope;
    g_aiDtorRuns = 0;
    void* mem = Object::detail::InternalMemAlloc(sizeof(AchievementInstance), 8);
    AchievementInstance* ai = ::new (mem) AchievementInstance();
    using Slot0 = Object::Base* (*)(void*, unsigned);
    Slot0 slot0 = reinterpret_cast<Slot0>(const_cast<void*>(g_rbAiVtGroup[2]));
    CHECK(static_cast<void*>(slot0(ai, 1u)) == mem);   // ~AchievementInstance, then MemFree
    CHECK_EQ(g_aiDtorRuns, 1);
}

UNIT_TEST(rb_achievement_bases_shape) {
    CHECK(!std::is_abstract<::Achievement::Achievement>::value);   // ENDGAME B.3.9 ENG
    CHECK(!std::is_destructible<::Achievement::Achievement>::value);
    CHECK(!std::has_virtual_destructor<::Achievement::Achievement>::value);
    CHECK(std::is_abstract<Goal>::value);                // slots 6-8 pure, as retail
    CHECK(!std::is_abstract<GoalInstance>::value);       // ENDGAME B.3.9 ENG
    CHECK(!std::is_destructible<Goal>::value);           // protected dtor
    CHECK(!std::is_destructible<GoalInstance>::value);
    CHECK(!std::has_virtual_destructor<GoalInstance>::value);
    CHECK(!std::is_abstract<AchievementInstance>::value);
    CHECK(!std::is_destructible<AchievementInstance>::value);   // protected dtor (B.3.9 ENG)
    CHECK(!std::has_virtual_destructor<AchievementInstance>::value);
    CHECK(!std::has_virtual_destructor<Goal>::value);
    CHECK_EQ(sizeof(RbScenario), (size_t)0x58);
    CHECK_EQ(sizeof(RbGoal), (size_t)0x30);
    CHECK_EQ(sizeof(RbGoalInstance), (size_t)0x20);
}

// ENDGAME B.3.9 ENG: Achievement's own slot 0 -- retail's VECTOR deleting
// destructor (element 0x58), both arms; two instances share the class table.
UNIT_TEST(b39_eng_achievement_deletingdtor_frees_both_arms) {
    using Ach = ::Achievement::Achievement;
    alignas(8) unsigned char x[0x58], y[0x58];
    Ach* a1 = ::new (static_cast<void*>(x)) Ach();
    Ach* a2 = ::new (static_cast<void*>(y)) Ach();
    RbScenario probe;
    CHECK(*reinterpret_cast<const void* const*>(a1) == *reinterpret_cast<const void* const*>(a2));
    CHECK(*reinterpret_cast<const void* const*>(a1) != *reinterpret_cast<const void* const*>(&probe));
    g_dtorCount = 0;
    a1->DeletingDtor(0);
    a2->DeletingDtor(0);
    CHECK_EQ(g_dtorCount, 2);
    CheckBothArms<Ach, 0x58>();
}

// ===========================================================================
// PART C
// ===========================================================================
}  // namespace
