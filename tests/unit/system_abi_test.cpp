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
#include "../../runtime/types/system/WideString.h"  // B.3.4b SYSTEM (4a), PART G
#include "../../runtime/types/system/Compression.h" // B.3.4b SYSTEM (4b), PART H
#include "../../runtime/types/system/Timer.h"   // B.3.4b SYSTEM (4), PART F
#include "../../runtime/types/system/Crc.h"
#include "../../runtime/types/system/Error.h"

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

// ===========================================================================
// PART F — B.3.4b SYSTEM (4): the all-static classes.
// ===========================================================================
// `System::Timer`, `System::CRC` and `System::Error` are classes whose every
// member is `static`. The mangling is the evidence and it is what separates
// them from B.3.4a's `IO::TextCodec`, which came out the other way — four
// receiverless free functions with NO exported class-scoped name, for which
// nothing landed. `?Startup@Timer@System@@SA_NXZ`, `?Compute@CRC@System@@SAIPEBXHI@Z`
// and `?SetText@Error@System@@SAXPEBD_N@Z` all carry `@<Class>@System@@` plus a
// static-member mangling (`SA` public static, `CA` private static, `0` private
// static data), which MSVC emits only for a member of a class of that name.
//
// They have NO instance state, so there is nothing to `offsetof` and no POD to
// cross-assert against — the whole of PART A has no analogue here. What IS
// worth pinning is that they stay empty and non-polymorphic, because a stray
// data member or virtual would silently give them a layout they must not have.

UNIT_TEST(the_all_static_system_classes_stay_empty_and_non_polymorphic) {
    CHECK(std::is_empty<System::Timer>::value);
    CHECK(std::is_empty<System::CRC>::value);
    CHECK(std::is_empty<System::Error>::value);
    CHECK(!std::is_polymorphic<System::Timer>::value);
    CHECK(!std::is_polymorphic<System::CRC>::value);
    CHECK(!std::is_polymorphic<System::Error>::value);
    // `System::Timer::operator=` is retail's only non-static member
    // (??4Timer@System@@QEAAAEAV01@AEBV01@@Z, 0x180007890, `MOV RAX,RCX; RET`).
    // It copies nothing, so the class stays trivially copyable and empty.
    CHECK(std::is_trivially_destructible<System::Timer>::value);
    CHECK(std::is_copy_assignable<System::Timer>::value);
}

UNIT_TEST(system_crc_compute_matches_the_standard_crc32_check_vector) {
    // THE ONE BEHAVIOURAL TEST IN THIS FILE, and it is possible because
    // `crc.cpp` is the only one of B.3.4b's six converted files that links on
    // the host: no <windows.h>, no RT_DATA_SLOT, no platform call, and its two
    // registration calls are stubbed. So this runs the REAL
    // `System::CRC::Compute` member, not a copy of it.
    //
    // "123456789" is the standard CRC-32 check vector and 0xCBF43926 is its
    // documented result. Retail's Compute does NOT complement the seed on
    // entry but DOES complement on exit (`NOT EAX` before the `RET`), so a
    // caller wanting the standard answer passes 0xFFFFFFFF itself — which is
    // exactly what this asserts, and what pins the entry/exit asymmetry that
    // Crc.h's banner describes.
    const char kCheck[] = "123456789";
    CHECK_EQ(System::CRC::Compute(kCheck, 9, 0xFFFFFFFFu), 0xCBF43926u);

    // The chaining convention `runtime/src/io/persistence_worker_win.cpp`'s
    // `Crc32Chunked` uses: feed the complement of the previous result back in.
    // Splitting the vector must reproduce the whole-buffer answer.
    uint32_t part = System::CRC::Compute(kCheck, 4, 0xFFFFFFFFu);
    CHECK_EQ(System::CRC::Compute(kCheck + 4, 5, ~part), 0xCBF43926u);

    // `count <= 0` skips the loop and still complements — retail's
    // `TEST EDX,EDX / JLE done`. Not an edge case nobody hits: it is the shape
    // of an empty chunk on the persistence path.
    CHECK_EQ(System::CRC::Compute(kCheck, 0, 0xFFFFFFFFu), 0x00000000u);
    CHECK_EQ(System::CRC::Compute(kCheck, -1, 0x12345678u), ~0x12345678u);
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

// ===========================================================================
// PART G — B.3.4b SYSTEM (4a): `System::WideString`.
// ===========================================================================
// This is the first 4b class whose POD and class are DELIBERATELY DIFFERENT in
// their special members, and that difference is the whole reason for the tests
// below. `RtSystem::WideString` stays a trivially destructible one-pointer POD,
// because four `runtime/src/` files hold one BY VALUE and release it through
// the registered `??1` door; `System::WideString` owns retail's release idiom
// and therefore has a real, non-trivial destructor. Two objects over the same
// eight bytes where exactly one of them destroys — asserted, so that "tidying"
// either direction fails the suite rather than the boot.
//
// NOTHING IS CONSTRUCTED OR CALLED HERE. `widestring.cpp` stands on the port
// registry (`RtMemAlloc` / `RtMemFree` reach `System::MemoryAllocator` through
// `rt_port.h`) and cannot link into a host test, exactly like `thread_win.cpp`
// and `value_types.cpp`. The UTF-8/UTF-16 round trip is verified by the real
// run; this part is layout and special members.

UNIT_TEST(system_widestring_is_8_bytes_and_aliases_the_pod_field_for_field) {
    CHECK_EQ(sizeof(System::WideString), static_cast<size_t>(8));
    CHECK_EQ(sizeof(System::WideString), sizeof(RtSystem::WideString));
    CHECK_EQ(alignof(System::WideString), alignof(RtSystem::WideString));
    CHECK_EQ(offsetof(System::WideString, _data), static_cast<size_t>(0x00));
    CHECK_EQ(offsetof(System::WideString, _data), offsetof(RtSystem::WideString, data));
    CHECK(!std::is_polymorphic<System::WideString>::value);
    CHECK(std::is_standard_layout<System::WideString>::value);

    // UTF-16, and the allocation header arithmetic in `SetLength`
    // (`8 + (len+1)*2`) is only right while this holds.
    CHECK_EQ(sizeof(wchar_t), static_cast<size_t>(2));
}

UNIT_TEST(the_widestring_class_destroys_and_the_widestring_pod_does_not) {
    // The class owns `??1WideString@System@@QEAA@XZ` (0x18000a730) — retail's
    // release idiom, asymmetry included. If this ever reads trivial, the
    // destructor has been deleted and the `??1` door releases nothing.
    CHECK(!std::is_trivially_destructible<System::WideString>::value);

    // The POD must stay trivial: `platform_startup_win.cpp`,
    // `application_win.cpp`, `updateloop_loadresources.cpp` and
    // `value_types.cpp` each hold one by value and call the door by hand. A
    // destructor here would run a SECOND release at scope exit — the
    // `RtSystem::String` double-free, B34_RULES 1.5 rule 3.
    CHECK(std::is_trivially_destructible<RtSystem::WideString>::value);

    // Retail exports no `??0WideString@System@@QEAA@XZ`, and an implicit
    // default constructor would let `System::WideString w;` leave `_data`
    // uninitialised for that destructor to release. The three real
    // constructors (0x1800cab50 / 0x18000a6e0 / 0x1800cb110) suppress it.
    CHECK(!std::is_default_constructible<System::WideString>::value);
    CHECK(std::is_trivially_default_constructible<RtSystem::WideString>::value);
}

UNIT_TEST(a_widestring_written_through_the_pod_reads_back_through_the_class) {
    // The live mixed state, as PART D does for Thread and PART E for the value
    // types: the four by-value holders write the POD spelling and the ported
    // bodies read the class spelling, on the same eight bytes.
    wchar_t chars[4] = { L'a', L'b', 0, 0 };

    RtSystem::WideString pod{};
    CHECK(pod.data == nullptr);
    pod.data = chars;

    System::WideString* asClass = reinterpret_cast<System::WideString*>(&pod);
    CHECK(asClass->_data == chars);
    CHECK_EQ(asClass->_data[0], L'a');

    asClass->_data = nullptr;          // what the sharing branch of Release does
    CHECK(pod.data == nullptr);
}

// ===========================================================================
// PART H — B.3.4b SYSTEM (4b): `System::Compression`.
// ===========================================================================
// The one 4b class whose live instance is a HEAP object and whose size is
// pinned by a call site rather than by a POD: both constructors in the tree do
// `MemAlloc(8, 4)` and hand those eight bytes to the `??0` door
// (`runtime/src/io/compressed_stream_write.cpp:629-633` and
// `runtime/src/save/saveload_meta.cpp:395-398`). Neither file can be told the
// class grew, so `sizeof == 8` is not a tidiness assert — it is the contract
// with two allocations that are out of this step's file set.
//
// Nothing is constructed here: the constructor allocates 0x40000 bytes through
// the port registry's MemoryAllocator door, so `compression.cpp` does not link
// on the host. The codec itself was verified against the shipped DLL by the
// Wave-9 / Wave-22 oracles and is verified today by every package the game
// loads.

UNIT_TEST(system_compression_is_one_pointer_and_must_stay_eight_bytes) {
    CHECK_EQ(sizeof(System::Compression), static_cast<size_t>(8));
    CHECK_EQ(offsetof(System::Compression, _table), static_cast<size_t>(0x00));
    CHECK(!std::is_polymorphic<System::Compression>::value);
    CHECK(std::is_standard_layout<System::Compression>::value);

    // `??1Compression@System@@` (0x1800c4170) is unported ON PURPOSE: the live
    // teardown is inlined into `IO::CompressedStream::Release`, which frees the
    // 0x40000 table and then the object itself through MemFree. A destructor
    // here would be a second, private opinion about a teardown that already
    // happens somewhere else — and it would run on top of it.
    CHECK(std::is_trivially_destructible<System::Compression>::value);

    // The constructor IS ported and IS registered, so the class is not
    // default-constructible-by-accident in the trivial sense: the door
    // placement-news it, which is what makes the 0x40000 table appear.
    CHECK(!std::is_trivially_default_constructible<System::Compression>::value);
}
