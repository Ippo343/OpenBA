// ============================================================================
// tests/unit/object_internal_test.cpp — ENDGAME B.3.3 step 5
//
// `Object::Internal` and `Object::InternalFactory`, the two real classes step 5
// introduces, driven on the host. B33_RULES.md §9 step 5 names the cases:
// layout against the alias PODs, the slot order of `??_7Internal@Object@@6B@`
// read through a raw `(*(void***)p)[N]`, `Type()`/`TypeName()` through
// Base -> Internal -> a derived test class, `Destroy(p)` reaching slot 0 with
// flags 1, and the factory's table logic.
//
// WHAT CAN AND CANNOT BE TESTED HERE
// ----------------------------------
// runtime/src/object/internal_class.cpp is registry-free under
// -DBANISHED_UNIT_TEST: its allocator seam is malloc, its `Rand` is a hook this
// file installs, and `InternalFactory::Get()` answers a factory this file owns.
// `ResetPersistState` and `PersistObject` are compiled OUT there — they need a
// live `IO::Stream` and the Factory type tree — so they are not covered.
//
// What only a live boot can prove, called out in the step's report:
//   * that the registered `RtObject_Internal_Ctor`/`_Dtor` shims, which still
//     stamp `VtblInternal()`/`VtblBase()` rather than the compiler's table,
//     drive the same handle table these members now compute —
//     `[RtInternalTable] ctorRuns=/dtorRuns=` in standalone_debug.log is the
//     counter that says so, and `[RtInternalVtbl]` is the line that says which
//     table got stamped;
//   * that `ClearPersistState`/`ResetPersistState` still write the same bytes
//     in the same order (`[RtPort] FIRST LIVE CALL into our ?ClearPersistState@`
//     plus the resave gate's byte-for-byte compare);
//   * post-`Factory::Startup` type ids. `Internal::TypeId()` is the 0xFFFFFFFF
//     ctor sentinel until Startup runs (B33_RULES §2.5), and Startup needs the
//     whole registered chain — including a record named "Base", whose absence
//     makes its parent pass fault BY DESIGN. So this file asserts the sentinel
//     and the names, not the ids.
// ============================================================================
#include "unit.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../../runtime/types/object/internal_class.h"

// The two unit-only seams runtime/src/object/internal_class.cpp defines under
// -DBANISHED_UNIT_TEST. They do not exist in the standalone build.
namespace Object {
namespace detail {
extern InternalFactory* g_unitInternalFactory;
extern uint32_t (*g_unitInternalRandHook)(::Random*);
}  // namespace detail
}  // namespace Object

namespace {

// ---------------------------------------------------------------------------
// Observation channel and the process-global guards.
// ---------------------------------------------------------------------------
int g_deletingDtorCalls = 0;
unsigned g_lastDeletingDtorFlags = 0;
int g_persistCalls = 0;
int g_derivedDtorCalls = 0;

uint32_t g_nextRand = 0;
uint32_t RandHook(::Random*) { return g_nextRand; }

// `InternalFactory::Get()` is one static pointer and the Rand hook is another.
// A test that installs either must restore it, or the next test in link order
// inherits it — the same rule object_typeinfo_test.cpp's ChainScope states for
// `_firstTypeInfo`.
struct FactoryScope {
    ::Object::InternalFactory  factory;
    ::Object::InternalFactory* saved;
    uint32_t (*savedRand)(::Random*);
    FactoryScope()
        : factory(),
          saved(::Object::detail::g_unitInternalFactory),
          savedRand(::Object::detail::g_unitInternalRandHook) {
        ::Object::detail::g_unitInternalFactory   = &factory;
        ::Object::detail::g_unitInternalRandHook  = &RandHook;
    }
    ~FactoryScope() {
        ::Object::detail::g_unitInternalFactory  = saved;
        ::Object::detail::g_unitInternalRandHook = savedRand;
    }
};

// ---------------------------------------------------------------------------
// A derived class, declared the way a converted subclass will be: DECLARE_TYPE,
// its own slot-0 override, no virtual destructor.
// ---------------------------------------------------------------------------
class TestInternal : public ::Object::Internal {
    DECLARE_TYPE(TestInternal)

public:
    TestInternal() : payload(0x5A5A5A5A) {}

    // Slot 0. A converted subclass writes exactly this: run its own destructor,
    // free on `flags & 1`, return `this`. Never `virtual ~TestInternal()`.
    ::Object::Base* DeletingDtor(unsigned flags) override {
        ++g_deletingDtorCalls;
        g_lastDeletingDtorFlags = flags;
        this->~TestInternal();
        if (flags & 1)
            ::Object::detail::InternalMemFree(this);
        return this;
    }

    // Slot 3.
    void Persist(void* /*stream*/) override { ++g_persistCalls; }

    uint32_t payload;

protected:
    ~TestInternal() { ++g_derivedDtorCalls; }
};

DEFINE_TYPE(TestInternal, "Internal", nullptr)

::Object::Base* TestInternal::CreateNewObject() { return nullptr; }
::Object::Base* TestInternal::CreateInPlaceObject(void*) { return nullptr; }

TestInternal* NewTestInternal() {
    void* storage = ::Object::detail::InternalMemAlloc(sizeof(TestInternal), 8);
    return new (storage) TestInternal();
}

// The four retail slot indices, spelled once.
enum : int { kSlotDeletingDtor = 0, kSlotTypeName = 1, kSlotType = 2, kSlotPersist = 3 };

void* const* VtableOf(const void* p) { return *reinterpret_cast<void* const* const*>(p); }

}  // namespace

// ===========================================================================
// Layout
// ===========================================================================

UNIT_TEST(internal_layout_matches_the_alias_pod) {
    static_assert(sizeof(::Object::Internal) == 0x10, "Object::Internal is 16 bytes");
    static_assert(sizeof(::Object::Internal) == sizeof(RtObject::Internal), "alias size");
    static_assert(offsetof(::Object::Internal, _factoryIndex) == 0x08, "+0x08");
    static_assert(offsetof(::Object::Internal, _generation)   == 0x0c, "+0x0c");
    static_assert(offsetof(::Object::Internal, _factoryIndex) ==
                      offsetof(RtObject::Internal, factoryIndex), "alias index");
    static_assert(offsetof(::Object::Internal, _generation) ==
                      offsetof(RtObject::Internal, generation), "alias generation");

    // The POD must stay trivially destructible — it is destroyed BY HAND out of
    // a vtable slot, so a destructor on it is a double free (B31_RULES).
    static_assert(std::is_trivially_destructible<RtObject::Internal>::value,
                  "the alias POD must stay trivially destructible");

    // No virtual destructor anywhere in the chain: GCC would spend two slots and
    // shift Type/TypeName/Persist by one. `std::has_virtual_destructor` is the
    // direct spelling of decision 2's rule.
    static_assert(!std::has_virtual_destructor< ::Object::Base>::value, "Base");
    static_assert(!std::has_virtual_destructor< ::Object::Internal>::value, "Internal");
    static_assert(!std::has_virtual_destructor<TestInternal>::value, "derived");

    CHECK_EQ(sizeof(::Object::Internal), (size_t)0x10);
}

UNIT_TEST(internalfactory_layout_matches_the_alias_pod) {
    static_assert(sizeof(::Object::InternalFactory) == 0x9e8, "0x9e4 fields, alignof 8");
    static_assert(sizeof(::Object::InternalFactory) == sizeof(RtObject::InternalFactory),
                  "alias size");
    static_assert(offsetof(::Object::InternalFactory, _objects)    == 0x00, "");
    static_assert(offsetof(::Object::InternalFactory, _persistIds) == 0x10, "");
    static_assert(offsetof(::Object::InternalFactory, _random)     == 0x20, "");
    CHECK_EQ(sizeof(::Object::InternalFactory), (size_t)0x9e8);
}

// ===========================================================================
// The vtable, read the way an unconverted C body reads it
// ===========================================================================

UNIT_TEST(internal_vtable_slot_order_is_retail) {
    FactoryScope scope;
    TestInternal* obj = NewTestInternal();

    void* const* vt = VtableOf(obj);

    // Slot 1 — `?TypeName@…@@UEBAPEBDXZ`, 0x1800064c0 in retail.
    const char* nameThroughSlot =
        reinterpret_cast<const char* (*)(const void*)>(vt[kSlotTypeName])(obj);
    CHECK_STREQ(nameThroughSlot, "TestInternal");
    CHECK_STREQ(nameThroughSlot, obj->TypeName());

    // Slot 2 — `?Type@…@@UEBAIXZ`, 0x1800064d0 in retail. 0xFFFFFFFF until
    // `Factory::Startup` runs, and this host never runs it.
    const uint32_t typeThroughSlot =
        reinterpret_cast<uint32_t (*)(const void*)>(vt[kSlotType])(obj);
    CHECK_EQ(typeThroughSlot, 0xFFFFFFFFu);
    CHECK_EQ(typeThroughSlot, obj->Type());
    CHECK_EQ(typeThroughSlot, TestInternal::TypeId());

    // Slot 3 — `?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z`, 0x1800bfda0.
    // `InternalFactory::PersistObject` reaches it as `vtbl[0x18]`, i.e. THIS
    // index, on both of its arms.
    const int before = g_persistCalls;
    reinterpret_cast<void (*)(void*, void*)>(vt[kSlotPersist])(obj, nullptr);
    CHECK_EQ(g_persistCalls, before + 1);

    ::Object::Destroy(obj);
}

UNIT_TEST(internal_persist_is_empty_on_the_base_class) {
    // 0x1800bfda0 is a bare RET in retail AND in the profile build. The only
    // thing to assert is that calling it does nothing observable and returns.
    FactoryScope scope;
    g_persistCalls = 0;
    TestInternal* obj = NewTestInternal();
    obj->Object::Internal::Persist(nullptr);   // the non-virtual, base spelling
    CHECK_EQ(g_persistCalls, 0);
    ::Object::Destroy(obj);
}

// ===========================================================================
// Type()/TypeName() through the three-deep chain
// ===========================================================================

UNIT_TEST(type_and_typename_through_base_internal_derived) {
    FactoryScope scope;
    TestInternal* derived = NewTestInternal();
    ::Object::Internal* asInternal = derived;
    ::Object::Base*     asBase     = derived;

    CHECK_STREQ(asBase->TypeName(), "TestInternal");
    CHECK_STREQ(asInternal->TypeName(), "TestInternal");
    CHECK_EQ(asBase->Type(), asInternal->Type());
    CHECK_EQ(asBase->Type(), derived->Type());

    // The MIDDLE of the chain has its own record, and it is retail's literal:
    // `?_TypeName@Internal@Object@@CAPEBDXZ` (0x18002fc60) returns "Internal"
    // and `?_ParentTypeName@` (0x18002fc70) returns "Base".
    CHECK_STREQ(::Object::Internal::GetTypeInfo().GetName(), "Internal");
    CHECK_STREQ(::Object::Internal::GetTypeInfo().GetParentName(), "Base");
    CHECK_STREQ(TestInternal::GetTypeInfo().GetName(), "TestInternal");
    CHECK_STREQ(TestInternal::GetTypeInfo().GetParentName(), "Internal");

    // The sentinel, pre-Startup, on every level (B33_RULES §2.5).
    CHECK_EQ(::Object::Internal::TypeId(), 0xFFFFFFFFu);
    CHECK_EQ(TestInternal::TypeId(), 0xFFFFFFFFu);

    // `?CreateNewObject@Internal@Object@@SAPEAVBase@2@XZ` (0x18002fc80) and
    // `?CreateInPlaceObject@…` (0x1800070c0) are both `XOR EAX,EAX; RET`.
    CHECK(::Object::Internal::CreateNewObject() == nullptr);
    CHECK(::Object::Internal::CreateInPlaceObject(nullptr) == nullptr);

    ::Object::Destroy(derived);
}

// ===========================================================================
// Destroy(p) -> slot 0 with flags 1
// ===========================================================================

UNIT_TEST(destroy_reaches_slot0_with_flags_1) {
    FactoryScope scope;
    g_deletingDtorCalls = 0;
    g_lastDeletingDtorFlags = 0;
    g_derivedDtorCalls = 0;

    TestInternal* obj = NewTestInternal();
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 1);

    ::Object::Destroy(obj);

    CHECK_EQ(g_deletingDtorCalls, 1);
    CHECK_EQ(g_lastDeletingDtorFlags, 1u);   // `&1` = free the storage
    CHECK_EQ(g_derivedDtorCalls, 1);
    // ~Internal ran too, so the object left the handle table.
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 0);

    // Destroy(nullptr) is a no-op, as the inline in TypeInfo.h says.
    ::Object::Destroy(nullptr);
    CHECK_EQ(g_deletingDtorCalls, 1);
}

// ===========================================================================
// InternalFactory — the handle table
// ===========================================================================

UNIT_TEST(factory_startup_and_shutdown_are_retails_two_instructions) {
    // 0x1800c0020 is `MOV AL,1; RET` and 0x1800c0010 is a bare `RET`. Retail
    // does not drain the table at shutdown; every object removes itself.
    FactoryScope scope;
    CHECK(scope.factory.Startup());
    scope.factory.Shutdown();
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 0);
}

UNIT_TEST(addobject_assigns_index_then_generation_then_grows) {
    FactoryScope scope;

    g_nextRand = 0x11111111u;
    TestInternal* a = NewTestInternal();
    CHECK_EQ(a->GetFactoryIndex(), 0);
    CHECK_EQ(a->GetGeneration(), 0x11111111u);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 1);

    g_nextRand = 0x22222222u;
    TestInternal* b = NewTestInternal();
    CHECK_EQ(b->GetFactoryIndex(), 1);
    CHECK_EQ(b->GetGeneration(), 0x22222222u);

    g_nextRand = 0x33333333u;
    TestInternal* c = NewTestInternal();
    CHECK_EQ(c->GetFactoryIndex(), 2);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 3);

    // The table holds the objects themselves, in index order.
    CHECK(scope.factory.GetObjects().GetData()[0] == static_cast< ::Object::Internal*>(a));
    CHECK(scope.factory.GetObjects().GetData()[1] == static_cast< ::Object::Internal*>(b));
    CHECK(scope.factory.GetObjects().GetData()[2] == static_cast< ::Object::Internal*>(c));

    // Retail's growth policy, from FUN_18002ffa0: the first growth takes
    // capacity to exactly newSize, every later +1 growth takes it to newSize*3.
    CHECK_EQ(scope.factory.GetObjects().GetCapacity(), 6);

    ::Object::Destroy(a);
    ::Object::Destroy(b);
    ::Object::Destroy(c);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 0);
}

UNIT_TEST(removeobject_swap_removes_from_the_middle_and_from_the_end) {
    FactoryScope scope;

    TestInternal* a = NewTestInternal();   // index 0
    TestInternal* b = NewTestInternal();   // index 1
    TestInternal* c = NewTestInternal();   // index 2
    CHECK_EQ(b->GetFactoryIndex(), 1);
    CHECK_EQ(c->GetFactoryIndex(), 2);

    // ---- the MIDDLE: last != idx, so the tail object moves down and has its
    // own `_factoryIndex` rewritten (0x1800c0139-0x1800c0140).
    ::Object::Destroy(b);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 2);
    CHECK(scope.factory.GetObjects().GetData()[1] == static_cast< ::Object::Internal*>(c));
    CHECK_EQ(c->GetFactoryIndex(), 1);
    CHECK(scope.factory.GetObjects().GetData()[0] == static_cast< ::Object::Internal*>(a));
    CHECK_EQ(a->GetFactoryIndex(), 0);

    // ---- the END: last == idx, so the swap arm is skipped entirely and only
    // the shrink runs.
    ::Object::Destroy(c);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 1);
    CHECK(scope.factory.GetObjects().GetData()[0] == static_cast< ::Object::Internal*>(a));
    CHECK_EQ(a->GetFactoryIndex(), 0);

    ::Object::Destroy(a);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 0);
    // The shrink keeps the buffer and the capacity — retail's SetSize never
    // shrinks capacity (Collection.h, and FUN_18002fca0's `_capacity < newSize`
    // test can only grow it).
    CHECK_EQ(scope.factory.GetObjects().GetCapacity(), 6);
}

UNIT_TEST(clearpersiststate_frees_and_zeroes_all_three_fields) {
    FactoryScope scope;

    // Give `_persistIds` a buffer through the same door ResetPersistState would
    // (0x18002fea0, element width 4), then clear it.
    ::Object::detail::ArrayIntSetSize(&scope.factory._persistIds, 8);
    CHECK_EQ(scope.factory._persistIds.GetSize(), 8);
    CHECK(scope.factory._persistIds.GetData() != nullptr);

    scope.factory.ClearPersistState();
    CHECK(scope.factory._persistIds.GetData() == nullptr);
    CHECK_EQ(scope.factory._persistIds.GetSize(), 0);
    CHECK_EQ(scope.factory._persistIds.GetCapacity(), 0);

    // It is idempotent on an already-empty array: retail's inlined free
    // null-checks first (and MemoryAllocator::Free opens with `test rdx,rdx`).
    scope.factory.ClearPersistState();
    CHECK(scope.factory._persistIds.GetData() == nullptr);
}

UNIT_TEST(array_reserve_grows_capacity_and_restores_size) {
    // FUN_18002fd90 — the one helper `ResetPersistState` uses that nothing else
    // reaches. It returns immediately unless n > _size, and it restores _size on
    // the way out, so only the capacity moves.
    FactoryScope scope;
    TestInternal* a = NewTestInternal();
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 1);

    ::Object::detail::ArrayPtrReserve(&scope.factory._objects, 32);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 1);
    CHECK_EQ(scope.factory.GetObjects().GetCapacity(), 32);
    CHECK(scope.factory.GetObjects().GetData()[0] == static_cast< ::Object::Internal*>(a));

    // n <= _size returns without touching anything.
    ::Object::detail::ArrayPtrReserve(&scope.factory._objects, 1);
    CHECK_EQ(scope.factory.GetObjects().GetCapacity(), 32);

    ::Object::Destroy(a);
}

UNIT_TEST(internal_copy_ctor_and_assign_copy_two_dwords_and_nothing_else) {
    // 0x1800064e0 and 0x180006430. Neither calls AddObject, so a copy shares the
    // ORIGINAL's `_factoryIndex` and the table count does not move. That is
    // retail, it is the swap-remove hazard's shape, and it is transcribed rather
    // than fixed.
    FactoryScope scope;
    g_nextRand = 0xABCDEF01u;
    TestInternal* a = NewTestInternal();
    const int sizeAfterOne = scope.factory.GetObjects().GetSize();

    // THE COPY IS NEVER DESTROYED, and that is the point rather than an
    // oversight. It shares `a`'s `_factoryIndex` without being in the table, so
    // whichever of the two is destroyed second runs `RemoveObject` with a stale
    // index against a shorter array — `_objects._data[idx] = _objects._data[-1]`
    // — which is precisely the wild write
    // reference_internalfactory_swapremove_wild_write.md is about. Retail
    // behaves this way and this port keeps it; the test observes the copy's
    // fields and then abandons the storage instead of exercising the fault.
    void* storage = ::Object::detail::InternalMemAlloc(sizeof(TestInternal), 8);
    TestInternal* copy = new (storage) TestInternal(*a);

    CHECK_EQ(scope.factory.GetObjects().GetSize(), sizeAfterOne);   // NOT 2
    CHECK_EQ(copy->GetFactoryIndex(), a->GetFactoryIndex());
    CHECK_EQ(copy->GetGeneration(), a->GetGeneration());
    // The copy carries the same compiler vtable — the copy ctor's two vptr
    // stores at 0x1800064f1/0x1800064fb are the constructor chain.
    CHECK(VtableOf(copy) == VtableOf(a));

    ::Object::Internal& lhs = *copy;
    lhs = *a;                              // Internal::operator=, 0x180006430
    CHECK_EQ(copy->GetFactoryIndex(), a->GetFactoryIndex());
    CHECK_EQ(copy->GetGeneration(), a->GetGeneration());
    CHECK_EQ(scope.factory.GetObjects().GetSize(), sizeAfterOne);

    ::Object::Destroy(a);
    CHECK_EQ(scope.factory.GetObjects().GetSize(), 0);

    // Raw free, no destructor: see above.
    ::Object::detail::InternalMemFree(storage);
}
