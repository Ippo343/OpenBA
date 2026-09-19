// ============================================================================
// tests/unit/system_abi_test.cpp — ENDGAME B.3.4b SYSTEM (2): the `System`
// coexistence proof, the sibling of `io_vtable_abi_test.cpp` and
// `object_vtable_abi_test.cpp`.
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// B.3.4b turns `System::Thread` and `System::CriticalSection` into real C++
// classes while the PODs (`RtSystem::Thread`, `RtSystem::CriticalSection`) stay
// live for every unconverted user, and while FOUR construction sites keep
// stamping a hand-built or minted vtable over +0x00:
//
//   code/Game/Game/pathfinder_seed.cpp:142-148     exe table 0x14015ef18
//   runtime/src/engine/singleton_ctors.cpp:448-450 0x180155860 over +0x10
//   standalone/rt_data_mint.cpp:434-461            ??_7Loader@Engine@@6B@
//   runtime/src/audio/audio_stream_win.cpp:303-341 Audio::Stream's own
//
// plus two readers of the BASE table 0x180155f20 — `thread_win.cpp`'s
// `RtSystemThreadVftable()` (by name) and
// `runtime/src/io/persistence_worker_win.cpp:585-589` (by address).
//
// So the step is only sound if GCC's compiler-generated vtable for a class
// declared per ENDGAME §B.1 decision 2 is interchangeable, slot for slot, with
// a hand-built `void*[2]` at retail's byte offsets — in BOTH directions:
//
//   A. a raw caller reading `(*(void***)t)[1]` off a REAL `System::Thread`
//      lands on that class's slot-1 override, `this` unadjusted;
//   B. a REAL class's ordinary virtual call on an object whose vptr was stamped
//      with a hand-built array of free functions lands on those free functions.
//
// Direction B is the one every live Thread depends on, because every live
// Thread in this build wears a MINTED or a DERIVED table, never GCC's.
//
// THE RETAIL SLOT LIST BEING MIRRORED
// -----------------------------------
// `??_7Thread@System@@6B@` @ 0x180155f20, TWO slots, from
// `standalone/rt_data_mint_tables.inc:3711-3717`:
//
//   [0] +0x00  0x18007ec28  `_purecall`  -> `virtual void Run() = 0`
//              The NAME is evidence, not a guess: the one derived table this
//              tree holds is `??_7Loader@Engine@@6B@`, whose slot-0 override is
//              exported as `?Run@Loader@Engine@@UEAAXXZ` @ 0x1800dbfc0
//              (standalone/rt_export_addrs.inc:3695).
//   [1] +0x08  0x18000c590  RT_MINT_NAMED
//              `?OnCompleted@Thread@System@@UEAAXXZ` -> `virtual void OnCompleted()`
//
// Full derivation and the rules: docs/runtime_port/endgame_b/B34B_RULES.md
// §1.1, §2 and rules B4b-1 / B4b-2 / B4b-3.
//
// WHAT THIS FILE DOES NOT TEST. Nothing behavioural: `Start`, `WaitForFinish`,
// `ThreadFunc`, `Lock` and `Unlock` all need live Win32 objects and are stubbed
// to `abort()` in `system_abi_stubs.cpp`. Their bodies are verified by the real
// run, not here. This file is about LAYOUT and SLOT ORDER, which is the part a
// build cannot catch on its own.
// ============================================================================

#include "unit.h"
#include "system_abi_stubs.h"

#include "../../runtime/types/system/Thread.h"
#include "../../runtime/types/system/Value.h"   // B.3.4b SYSTEM (3), PART E

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace {

// ---------------------------------------------------------------------------
// The probe. Concrete, so it can be constructed; overrides BOTH slots, so a
// raw slot call can say which slot it landed in.
// ---------------------------------------------------------------------------
int g_probeRunCalls = 0;
int g_probeOnCompletedCalls = 0;

struct ThreadProbe : System::Thread {
    void Run() override         { ++g_probeRunCalls; }
    void OnCompleted() override { ++g_probeOnCompletedCalls; }
};

// The hand-built table direction B drives — the exact shape
// `standalone/rt_data_mint_tables.inc` publishes: an array of two `void*`,
// slot 0 then slot 1, no offset-to-top and no RTTI pointer in play because
// nothing reads them.
int g_freeRunCalls = 0;
int g_freeOnCompletedCalls = 0;

void FreeRun(void* /*self*/)         { ++g_freeRunCalls; }
void FreeOnCompleted(void* /*self*/) { ++g_freeOnCompletedCalls; }

void* g_mintedThreadTable[2] = { reinterpret_cast<void*>(&FreeRun),
                                 reinterpret_cast<void*>(&FreeOnCompleted) };

using SlotFn = void (*)(void*);

// Read slot `n` out of whatever `p`'s vptr points at, the way every
// unconverted caller in the tree does it.
SlotFn RawSlot(void* p, int n) {
    void** vt = *reinterpret_cast<void***>(p);
    return reinterpret_cast<SlotFn>(vt[n]);
}

}  // namespace

// ===========================================================================
// PART A — layout: the class and the POD are the same bytes.
// ===========================================================================
// `Thread.h` already static_asserts all of this, so a regression is a BUILD
// failure, not a test failure. It is restated at runtime anyway because a
// static_assert that someone deletes leaves no trace, and a named test that
// disappears from the 235-line output does.

UNIT_TEST(system_thread_is_0x30_bytes_and_aliases_the_pod_field_for_field) {
    CHECK_EQ(sizeof(System::Thread), static_cast<size_t>(0x30));
    CHECK_EQ(sizeof(System::Thread), sizeof(RtSystem::Thread));

    CHECK_EQ(offsetof(System::Thread, _runFlags),  static_cast<size_t>(0x08));
    CHECK_EQ(offsetof(System::Thread, _pad0c),     static_cast<size_t>(0x0c));
    CHECK_EQ(offsetof(System::Thread, _hThread),   static_cast<size_t>(0x10));
    CHECK_EQ(offsetof(System::Thread, _hStart),    static_cast<size_t>(0x18));
    CHECK_EQ(offsetof(System::Thread, _hStop),     static_cast<size_t>(0x20));
    CHECK_EQ(offsetof(System::Thread, _hFinished), static_cast<size_t>(0x28));

    CHECK_EQ(offsetof(System::Thread, _runFlags),  offsetof(RtSystem::Thread, runFlags));
    CHECK_EQ(offsetof(System::Thread, _hThread),   offsetof(RtSystem::Thread, hThread));
    CHECK_EQ(offsetof(System::Thread, _hStart),    offsetof(RtSystem::Thread, hStart));
    CHECK_EQ(offsetof(System::Thread, _hStop),     offsetof(RtSystem::Thread, hStop));
    CHECK_EQ(offsetof(System::Thread, _hFinished), offsetof(RtSystem::Thread, hFinished));

    // `vtable` is the one member with no cross-assert: the POD spells +0x00 as
    // a field, the class lets the compiler own it. This is the assertion that
    // says the two nonetheless agree about WHERE it is.
    CHECK_EQ(offsetof(RtSystem::Thread, vtable), static_cast<size_t>(0x00));
}

UNIT_TEST(system_thread_base_is_0x30_so_engine_loader_loop_stays_at_0x30) {
    // `RtEngine::Loader` (runtime/types/engine/UpdateLoop.h) is `Thread` at
    // +0x00 and `_loop` at +0x30, and `ConstructLoader`
    // (standalone/rt_data_mint.cpp:434-461) writes `_loop` by hand at that
    // literal offset. That offset is only correct while the base is 0x30 — the
    // same pin `rt_data_mint.cpp:149` carries. Restated here so B.3.4b's
    // re-spelling of the member is covered by `make unit` as well as by the
    // standalone build.
    CHECK_EQ(sizeof(RtSystem::Thread), static_cast<size_t>(0x30));
    CHECK_EQ(sizeof(System::Thread),   static_cast<size_t>(0x30));
}

UNIT_TEST(system_criticalsection_is_a_plain_0x28_byte_blob_with_no_vptr) {
    CHECK_EQ(sizeof(System::CriticalSection), static_cast<size_t>(0x28));
    CHECK_EQ(sizeof(System::CriticalSection), sizeof(RtSystem::CriticalSection));
    CHECK_EQ(alignof(System::CriticalSection), alignof(RtSystem::CriticalSection));
    CHECK(!std::is_polymorphic<System::CriticalSection>::value);
    CHECK(std::is_standard_layout<System::CriticalSection>::value);
    // B4b-2: teardown is the registered `??1CriticalSection@System@@` door, not
    // an implicit member destructor. Two of them would DeleteCriticalSection
    // twice — the `RtSystem::String` double-free in a new costume.
    CHECK(std::is_trivially_destructible<System::CriticalSection>::value);
    CHECK(std::is_trivially_destructible<RtSystem::CriticalSection>::value);
}

// ===========================================================================
// PART B — the shape decision 2 asks for, enforced.
// ===========================================================================

UNIT_TEST(system_thread_is_abstract_which_is_what_forbids_constructing_one) {
    // B4b-2/B4b-3. A concrete polymorphic class would have an implicit PUBLIC
    // default constructor, so `System::Thread t;` would compile and stamp GCC's
    // vtable on an object every live path expects to be wearing the MINTED one
    // — with no build error at all. Abstract is what makes that a hard compile
    // error, and retail's slot 0 really is `_purecall`, so abstract is also the
    // faithful shape rather than a trick.
    CHECK(std::is_abstract<System::Thread>::value);
    CHECK(std::is_polymorphic<System::Thread>::value);
    CHECK(!std::is_default_constructible<System::Thread>::value);

    // The POD is NOT abstract and NOT polymorphic — it is what the four
    // construction sites and `ThreadFunc` still write `self->vtable` through.
    CHECK(!std::is_polymorphic<RtSystem::Thread>::value);
    CHECK(std::is_trivially_destructible<RtSystem::Thread>::value);
}

UNIT_TEST(a_virtual_destructor_would_shift_oncompleted_off_slot_1) {
    // ENDGAME decision 2, stated as a measurement rather than a comment.
    // Itanium spends TWO slots on a virtual destructor, so declaring one on
    // `System::Thread` would push `OnCompleted` from slot 1 to slot 3 and
    // silently mis-aim `ThreadFunc`'s `vtAfter[1]` and every minted row that
    // fills slot 1 with 0x18000c590. The guard is that slot 1 of a real probe
    // IS the slot-1 override (PART C below proves it) AND that the class has no
    // virtual destructor to begin with.
    CHECK(!std::has_virtual_destructor<System::Thread>::value);
}

// ===========================================================================
// PART C — DIRECTION A: a raw slot call on a REAL class lands on its override.
// ===========================================================================

UNIT_TEST(raw_slot_0_and_slot_1_on_a_real_thread_reach_run_and_oncompleted) {
    ThreadProbe probe;
    g_probeRunCalls = 0;
    g_probeOnCompletedCalls = 0;

    // Exactly what `ThreadFunc` (0x1800ca440) does on every worker pass, and
    // what `persistence_worker_win.cpp` and the pathfinder do to a Thread they
    // built themselves.
    RawSlot(&probe, 0)(&probe);
    CHECK_EQ(g_probeRunCalls, 1);
    CHECK_EQ(g_probeOnCompletedCalls, 0);

    RawSlot(&probe, 1)(&probe);
    CHECK_EQ(g_probeRunCalls, 1);
    CHECK_EQ(g_probeOnCompletedCalls, 1);

    // `this` unadjusted: single inheritance, one vptr, and the object's address
    // IS the vptr's address.
    CHECK_EQ(static_cast<void*>(&probe),
             static_cast<void*>(static_cast<System::Thread*>(&probe)));
}

UNIT_TEST(a_real_thread_has_exactly_two_reachable_slots_in_declaration_order) {
    // There is no portable "count the slots" operation, so this is the strongest
    // statement available: slot 0 and slot 1 are the two overrides, they are
    // DIFFERENT entries, and they are in declaration order. A third virtual
    // inserted anywhere but the end, or a reordering, breaks one of the three.
    ThreadProbe probe;
    void** vt = *reinterpret_cast<void***>(&probe);
    CHECK(vt[0] != nullptr);
    CHECK(vt[1] != nullptr);
    CHECK(vt[0] != vt[1]);

    g_probeRunCalls = 0;
    g_probeOnCompletedCalls = 0;
    reinterpret_cast<SlotFn>(vt[0])(&probe);
    reinterpret_cast<SlotFn>(vt[1])(&probe);
    CHECK_EQ(g_probeRunCalls, 1);
    CHECK_EQ(g_probeOnCompletedCalls, 1);
}

// ===========================================================================
// PART D — DIRECTION B: a real class's virtual call on a MINTED table.
// ===========================================================================
// This is the direction every live Thread in the build depends on, because not
// one of them wears GCC's table.

UNIT_TEST(a_minted_two_slot_table_drives_the_real_class_virtual_call) {
    RtSystem::Thread pod{};
    pod.vtable = static_cast<void*>(g_mintedThreadTable);

    System::Thread* asClass = reinterpret_cast<System::Thread*>(&pod);

    g_freeRunCalls = 0;
    g_freeOnCompletedCalls = 0;

    asClass->Run();                 // pure in the class -> slot 0 of the table
    CHECK_EQ(g_freeRunCalls, 1);
    CHECK_EQ(g_freeOnCompletedCalls, 0);

    asClass->OnCompleted();         // slot 1 of the table, NOT the stub body
    CHECK_EQ(g_freeRunCalls, 1);
    CHECK_EQ(g_freeOnCompletedCalls, 1);
}

UNIT_TEST(the_qualified_call_suppresses_dispatch_and_reaches_the_class_body) {
    // B34_BRIEFS §A.1, which is why `RtSystem_Thread_OnCompleted` writes
    // `self->System::Thread::OnCompleted()`. On an object wearing the minted
    // table, whose slot 1 resolves back to that very door, an UNqualified call
    // would recurse forever. The qualification has to reach the class body
    // instead — here, the stub standing in for it.
    RtSystem::Thread pod{};
    pod.vtable = static_cast<void*>(g_mintedThreadTable);

    System::Thread* asClass = reinterpret_cast<System::Thread*>(&pod);

    g_freeOnCompletedCalls = 0;
    const int before = g_systemStub_OnCompletedCalls;

    asClass->System::Thread::OnCompleted();

    CHECK_EQ(g_freeOnCompletedCalls, 0);                    // did NOT dispatch
    CHECK_EQ(g_systemStub_OnCompletedCalls, before + 1);    // reached the body
}

// ===========================================================================
// PART E — B.3.4b SYSTEM (3): the value types.
// ===========================================================================
// `System::Random`, `Time`, `Color` and `DateTime` have NO vtable, so there is
// no slot order to pin and nothing here resembles PARTS C and D. What there IS
// to pin is the thing a build can get wrong silently: that each class is the
// same bytes as the POD the unconverted tree still reads through a cast, and
// that none of them acquires a vptr, a virtual, or a destructor.
//
// NO MEMBER IS CALLED HERE, deliberately. Their bodies live in
// `runtime/src/system/value_types.cpp`, which includes <windows.h> and stands
// on the port registry (`RT_EXPORT_ONCE` for `Timer::TicksToSeconds`,
// `String::Format`, the two DateTime siblings) and cannot link into a host test
// — the same reason `object_ptr_test.cpp` and `set_test.cpp` are header-only.
// The MT19937 draw, the tick conversion and the three date formats are verified
// by the real run. This part is layout, and layout is what a static_assert can
// carry but a deleted static_assert cannot report.

UNIT_TEST(system_random_is_0x9c4_bytes_and_aliases_the_global_pod) {
    CHECK_EQ(sizeof(System::Random), static_cast<size_t>(2500));
    CHECK_EQ(sizeof(System::Random), sizeof(::Random));
    CHECK_EQ(offsetof(System::Random, _index), static_cast<size_t>(0x00));
    CHECK_EQ(offsetof(System::Random, _mt),    static_cast<size_t>(0x04));
    CHECK_EQ(offsetof(System::Random, _index), offsetof(::Random, index));
    CHECK_EQ(offsetof(System::Random, _mt),    offsetof(::Random, mt));
    CHECK(!std::is_polymorphic<System::Random>::value);
    CHECK(std::is_standard_layout<System::Random>::value);
    CHECK(std::is_trivially_destructible<System::Random>::value);

    // B4b-2, and for Random the reason is specific: `Object::InternalFactory`
    // holds a `::Random` BY VALUE at +0x20 (runtime/types/object/Internal.h)
    // while `Ctor_InternalFactory` seeds it through the
    // `??0Random@System@@QEAA@XZ` door. A real constructor here would seed the
    // object factory's RNG twice from two different `time(nullptr)` values the
    // moment anyone re-spells that member — a determinism change with no build
    // error. This assertion is what makes adding one fail the test suite.
    CHECK(std::is_trivially_default_constructible<System::Random>::value);
}

UNIT_TEST(system_time_is_8_bytes_over_the_shared_global_struct_Time) {
    // `::Time` is `engine/types/common.h`'s, shared with the exe tree, and is
    // NOT moved by B.3.4b (rule B4b-1 does not fire: the POD never occupied the
    // class's name). `System::Time` is a new name over the same eight bytes.
    CHECK_EQ(sizeof(System::Time), static_cast<size_t>(8));
    CHECK_EQ(sizeof(System::Time), sizeof(::Time));
    CHECK_EQ(offsetof(System::Time, _ticks), static_cast<size_t>(0x00));
    CHECK_EQ(offsetof(System::Time, _ticks), offsetof(::Time, ticks));
    CHECK(!std::is_polymorphic<System::Time>::value);
    CHECK(std::is_standard_layout<System::Time>::value);
    // The ~132 `fn_Time_Ctor_Float` sites build these in RAW BYTE BUFFERS and
    // convert as placement-new; a non-trivial default constructor or a
    // destructor would change what those buffers cost.
    CHECK(std::is_trivially_default_constructible<System::Time>::value);
    CHECK(std::is_trivially_destructible<System::Time>::value);
}

UNIT_TEST(system_color_is_4_bytes_and_aliases_the_global_pod) {
    CHECK_EQ(sizeof(System::Color), static_cast<size_t>(4));
    CHECK_EQ(sizeof(System::Color), sizeof(::Color));
    CHECK_EQ(offsetof(System::Color, _value), static_cast<size_t>(0x00));
    CHECK_EQ(offsetof(System::Color, _value), offsetof(::Color, value));
    CHECK(!std::is_polymorphic<System::Color>::value);
    CHECK(std::is_standard_layout<System::Color>::value);
    CHECK(std::is_trivially_destructible<System::Color>::value);
}

UNIT_TEST(system_datetime_is_a_filetime_two_dwords_not_an_int64) {
    // The one class in Value.h that had NO POD at all — the header used to say
    // "`this` IS A FILETIME, 8 opaque bytes" and every body took `const void*`.
    // A FILETIME is {DWORD dwLowDateTime; DWORD dwHighDateTime;}, so it is
    // 4-ALIGNED. Spelling it `uint64_t` would compile, would keep sizeof at 8,
    // and would silently raise alignof to 8 — which is why alignment is
    // asserted here and not only size.
    CHECK_EQ(sizeof(System::DateTime), static_cast<size_t>(8));
    CHECK_EQ(alignof(System::DateTime), static_cast<size_t>(4));
    CHECK_EQ(offsetof(System::DateTime, _lowDateTime),  static_cast<size_t>(0x00));
    CHECK_EQ(offsetof(System::DateTime, _highDateTime), static_cast<size_t>(0x04));
    CHECK(!std::is_polymorphic<System::DateTime>::value);
    CHECK(std::is_standard_layout<System::DateTime>::value);
    CHECK(std::is_trivially_destructible<System::DateTime>::value);
}

UNIT_TEST(the_value_classes_write_the_same_bytes_the_pods_read) {
    // The live mixed state, as PART D does for Thread: written through the POD
    // spelling (what the unconverted tree does) and read back through the
    // class, and the reverse. This is the whole coexistence claim for a class
    // with no vtable.
    ::Random pod{};
    pod.index = 42;
    pod.mt[0] = 0xdeadbeefu;
    pod.mt[623] = 0x0badc0deu;
    System::Random* asClass = reinterpret_cast<System::Random*>(&pod);
    CHECK_EQ(asClass->_index, 42);
    CHECK_EQ(asClass->_mt[0], 0xdeadbeefu);
    CHECK_EQ(asClass->_mt[623], 0x0badc0deu);
    asClass->_index = 7;
    CHECK_EQ(pod.index, 7);

    ::Color cpod{};
    cpod.value = 0xff3366ccu;   // AARRGGBB as a uint32, per Value.h's banner
    System::Color* cAsClass = reinterpret_cast<System::Color*>(&cpod);
    CHECK_EQ(cAsClass->_value, cpod.value);

    ::Time tpod{};
    tpod.ticks = 0x0123456789abcdefLL;
    System::Time* tAsClass = reinterpret_cast<System::Time*>(&tpod);
    CHECK_EQ(tAsClass->_ticks, 0x0123456789abcdefLL);
}

UNIT_TEST(the_pod_and_the_class_agree_about_which_bytes_the_fields_are) {
    // The live mixed state in one test: a POD written through the POD spelling
    // (what `RtSystem_Thread_Ctor` and `ThreadFunc` do) and read back through
    // the CLASS spelling (what `Start`/`WaitForFinish` do), and the reverse.
    RtSystem::Thread pod{};
    pod.vtable    = static_cast<void*>(g_mintedThreadTable);
    pod.runFlags  = 0x5a5a5a5au;
    pod.hThread   = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1111));
    pod.hStart    = reinterpret_cast<void*>(static_cast<uintptr_t>(0x2222));
    pod.hStop     = reinterpret_cast<void*>(static_cast<uintptr_t>(0x3333));
    pod.hFinished = reinterpret_cast<void*>(static_cast<uintptr_t>(0x4444));

    System::Thread* asClass = reinterpret_cast<System::Thread*>(&pod);
    CHECK_EQ(asClass->_runFlags, 0x5a5a5a5au);
    CHECK_EQ(asClass->_hThread,   pod.hThread);
    CHECK_EQ(asClass->_hStart,    pod.hStart);
    CHECK_EQ(asClass->_hStop,     pod.hStop);
    CHECK_EQ(asClass->_hFinished, pod.hFinished);

    asClass->_runFlags |= 1u;               // what `ThreadFunc` sets
    CHECK_EQ(pod.runFlags, 0x5a5a5a5bu);
    asClass->_runFlags &= ~1u;
    CHECK_EQ(pod.runFlags, 0x5a5a5a5au);
}
