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
// PART B — DOOR == CTOR: the same bytes come out of the door shape (stand-in
//   minted stamp + the inherited base's stores + `DoorBody::Init`).
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
Achievement::~Achievement() {}

const char* AchievementInstance::TypeName() const { return "AchievementInstance"; }
uint32_t    AchievementInstance::Type() const { return 0; }
void        AchievementInstance::Persist(void*) {}
AchievementInstance::~AchievementInstance() { ++g_aiDtorRuns; }

const char* Goal::TypeName() const { return "Goal"; }
uint32_t    Goal::Type() const { return 0; }
int32_t     Goal::GetFileVersion() const { return 0x10000; }
bool        Goal::Serialize(void*) { return true; }
Object::Type* Goal::GetInstanceType(Object::Type* ret) const { return ret; }
Goal::~Goal() {}

const char* GoalInstance::TypeName() const { return "GoalInstance"; }
uint32_t    GoalInstance::Type() const { return 0; }
void        GoalInstance::Persist(void*) {}
void        GoalInstance::Startup() {}
void        GoalInstance::Update(const System::Time&, const GoalList&) {}
Goal::GoalState GoalInstance::CheckGoalState() const { return Goal::kNotMet; }
GoalInstance::~GoalInstance() {}

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
const void* const kFakeMinted[4] = {};

// ===========================================================================
// PART A + B
// ===========================================================================
UNIT_TEST(rb_achievement_ctor_is_retail_0x18009e240_and_the_door_body) {
    alignas(8) unsigned char a[sizeof(RbScenario)];
    RbScenario* s = BuildIn<RbScenario>(a);
    for (size_t off = 0x08; off < 0x58; off += 8) CHECK_EQ(Q(s, off), (uint64_t)0);

    alignas(8) unsigned char b[sizeof(RbScenario)];
    std::memset(b, 0xCD, sizeof b);
    *reinterpret_cast<const void**>(b) = kFakeMinted;
    std::memset(b + 0x08, 0, 0x18);   // the door's inlined External stores
    ::Achievement::Achievement::DoorBody::Init(reinterpret_cast<::Achievement::Achievement*>(b));
    CHECK(std::memcmp(a + 8, b + 8, sizeof(RbScenario) - 8) == 0);
    s->~RbScenario();
}

UNIT_TEST(rb_goal_ctor_is_retail_0x18009ed50_and_the_door_body) {
    alignas(8) unsigned char a[sizeof(RbGoal)];
    RbGoal* g = BuildIn<RbGoal>(a);
    for (size_t off = 0x08; off < 0x30; off += 8) CHECK_EQ(Q(g, off), (uint64_t)0);

    alignas(8) unsigned char b[sizeof(RbGoal)];
    std::memset(b, 0xCD, sizeof b);
    *reinterpret_cast<const void**>(b) = kFakeMinted;
    std::memset(b + 0x08, 0, 0x18);
    Goal::DoorBody::Init(reinterpret_cast<Goal*>(b));
    CHECK(std::memcmp(a + 8, b + 8, sizeof(RbGoal) - 8) == 0);
    g->~RbGoal();
}

UNIT_TEST(rb_goalinstance_ctor_is_retail_0x18009ef00_and_the_door_body) {
    FactoryScope scope;
    alignas(8) unsigned char a[sizeof(RbGoalInstance)];
    RbGoalInstance* g = BuildIn<RbGoalInstance>(a);
    CHECK_EQ(D(g, 0x10), 0u);             // _flags (a DWORD store)
    CHECK_EQ(D(g, 0x14), 0xCDCDCDCDu);    // +0x14 NEVER written
    CHECK_EQ(Q(g, 0x18), (uint64_t)0);    // _goal

    alignas(8) unsigned char b[sizeof(RbGoalInstance)];
    std::memcpy(b, a, 0x10);              // the same Internal half
    std::memset(b + 0x10, 0xCD, sizeof b - 0x10);
    *reinterpret_cast<const void**>(b) = kFakeMinted;
    GoalInstance::DoorBody::Init(reinterpret_cast<GoalInstance*>(b));
    CHECK(std::memcmp(a + 0x10, b + 0x10, sizeof(RbGoalInstance) - 0x10) == 0);
    g->~RbGoalInstance();
}

UNIT_TEST(rb_achievementinstance_ctor_is_retail_0x18009e3b0) {
    FactoryScope scope;
    alignas(8) unsigned char a[sizeof(AchievementInstance)];
    std::memset(a, 0xCD, sizeof a);
    AchievementInstance* ai = ::new (static_cast<void*>(a)) AchievementInstance();
    CHECK(*reinterpret_cast<const void* const*>(ai) == &g_rbAiVtGroup[2]);
    for (size_t off = 0x10; off < 0x28; off += 8) CHECK_EQ(Q(ai, off), (uint64_t)0);

    alignas(8) unsigned char b[sizeof(AchievementInstance)];
    std::memcpy(b, a, 0x10);
    std::memset(b + 0x10, 0xCD, sizeof b - 0x10);
    *reinterpret_cast<const void**>(b) = kFakeMinted;
    AchievementInstance::DoorBody::Init(reinterpret_cast<AchievementInstance*>(b));
    CHECK(std::memcmp(a + 0x10, b + 0x10, sizeof(AchievementInstance) - 0x10) == 0);
    ai->DeletingDtor(0);   // destruct only (flags 0: no free of a stack buffer)
}

// ===========================================================================
// PART C
// ===========================================================================
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
    CHECK(std::is_abstract<::Achievement::Achievement>::value);
    CHECK(std::is_abstract<Goal>::value);
    CHECK(std::is_abstract<GoalInstance>::value);
    CHECK(!std::is_abstract<AchievementInstance>::value);
    CHECK((std::is_default_constructible<AchievementInstance>::value));
    CHECK(!std::has_virtual_destructor<AchievementInstance>::value);
    CHECK(!std::has_virtual_destructor<Goal>::value);
    CHECK_EQ(sizeof(RbScenario), (size_t)0x58);
    CHECK_EQ(sizeof(RbGoal), (size_t)0x30);
    CHECK_EQ(sizeof(RbGoalInstance), (size_t)0x20);
}

}  // namespace
