// ============================================================================
// tests/unit/b39_sys_thread_test.cpp — ENDGAME B.3.9 SYS W1: the REAL
// `System::Thread(unsigned)` ctor and protected non-virtual `~Thread()`.
// ============================================================================
// Retail: ??0Thread@System@@QEAA@I@Z 0x1800ca4e0 / ??1Thread@System@@QEAA@XZ
// 0x1800ca590. Both are "store the base vptr, then the body"; the class's
// ctor/dtor leave the store to the compiler and share the body with the two
// registered doors through `InitFields` / `ReleaseFields` (RULE B8-1).
//
// No OS thread is started here (the existing Thread tests never start one):
// the host link gets RECORDING `InitFields` / `ReleaseFields` from
// tests/unit/system_abi_stubs.cpp, so these tests pin WHEN the shared body
// runs, how often, on which object, with which argument, and which table the
// object wears at that moment. The body itself (the _beginthreadex / four
// CreateEventW / ResumeThread, and the SetEvent / wait / four CloseHandle) is
// verified by the run: every Thread in the build still goes through it.
//
// There is NO `DeletingDtor` test: retail's `??_7Thread@System@@6B@` has two
// slots, `Run` (`_purecall`) and `OnCompleted`, and no deleting-dtor slot —
// nor do its subclasses' tables (Engine::Loader, Audio::Stream: "NO deleting
// dtor"). Declaring one would add a third slot the minted table does not have.
// ============================================================================

#include "unit.h"
#include "system_abi_stubs.h"
#include "../../runtime/types/system/Thread.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {

void* VptrOf(const void* p) { return *static_cast<void* const*>(p); }

// Itanium pointer-to-virtual-member: { ptr = 1 + vtable_byte_offset, adj }.
template <typename PMF>
long ThreadSlotOf(PMF pmf, uintptr_t* adj) {
    static_assert(sizeof(PMF) == 2 * sizeof(void*), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof(words));
    *adj = words[1];
    if ((words[0] & 1u) == 0) return -1;
    return static_cast<long>((words[0] - 1) / sizeof(void*));
}

int   g_realRunCalls = 0;
void* g_vptrInDerivedCtorBody = nullptr;
void* g_vptrInDerivedDtorBody = nullptr;

// A leaf built with the REAL base ctor, the way Engine::Loader
// (`: System::Thread(0x40000)`) and the other subclasses will be in W2.
struct RealCtorThread final : System::Thread {
    explicit RealCtorThread(unsigned int stackSize) : System::Thread(stackSize) {
        g_vptrInDerivedCtorBody = VptrOf(this);
    }
    ~RealCtorThread() { g_vptrInDerivedDtorBody = VptrOf(this); }
    void Run() override { ++g_realRunCalls; }
};

void ResetRecords() {
    g_systemStub_InitFields    = {};
    g_systemStub_ReleaseFields = {};
    g_vptrInDerivedCtorBody = nullptr;
    g_vptrInDerivedDtorBody = nullptr;
}

}  // namespace

UNIT_TEST(b39_sys_thread_shape) {
    CHECK(std::is_abstract<System::Thread>::value);              // retail slot 0 is _purecall
    CHECK(std::is_polymorphic<System::Thread>::value);
    CHECK(!std::is_destructible<System::Thread>::value);         // protected dtor (D1.3)
    CHECK(!std::has_virtual_destructor<System::Thread>::value);  // non-virtual until the cutover
    CHECK_EQ(sizeof(System::Thread), static_cast<size_t>(0x30));
    CHECK((std::is_constructible<RealCtorThread, unsigned int>::value));
    CHECK(!std::is_abstract<RealCtorThread>::value);
}

UNIT_TEST(b39_sys_thread_slot_order_via_pmf) {
    uintptr_t adj = 99;
    CHECK_EQ(ThreadSlotOf(&System::Thread::Run, &adj), 0L);
    CHECK_EQ(adj, static_cast<uintptr_t>(0));
    adj = 99;
    CHECK_EQ(ThreadSlotOf(&System::Thread::OnCompleted, &adj), 1L);
    CHECK_EQ(adj, static_cast<uintptr_t>(0));
}

UNIT_TEST(b39_sys_thread_real_ctor_runs_the_shared_body_once_under_the_base_table) {
    ResetRecords();
    {
        RealCtorThread t(0x40000);

        // ONE call of the door's body, on this object, with the argument.
        CHECK_EQ(g_systemStub_InitFields.calls, 1);
        CHECK(g_systemStub_InitFields.self == static_cast<const void*>(&t));
        CHECK_EQ(g_systemStub_InitFields.stackSize, 0x40000u);

        // Stamp order: the body ran while the object wore System::Thread's OWN
        // table (retail stores 0x180155f20 before the body), and the leaf's
        // table replaced it afterwards.
        void* baseTable = g_systemStub_InitFields.vptrAtCall;
        void* leafTable = VptrOf(&t);
        CHECK(baseTable != nullptr);
        CHECK(baseTable != leafTable);
        CHECK(g_vptrInDerivedCtorBody == leafTable);

        // The base table is Thread's: slot 1 is the inherited OnCompleted (the
        // leaf does not override it), slot 0 differs (pure there, Run here).
        void** base = static_cast<void**>(baseTable);
        void** leaf = static_cast<void**>(leafTable);
        CHECK(base[1] == leaf[1]);
        CHECK(base[0] != leaf[0]);

        // The leaf's slot 0 is its Run.
        g_realRunCalls = 0;
        reinterpret_cast<void (*)(void*)>(leaf[0])(&t);
        CHECK_EQ(g_realRunCalls, 1);

        CHECK_EQ(g_systemStub_ReleaseFields.calls, 0);
    }
}

UNIT_TEST(b39_sys_thread_dtor_runs_the_shared_teardown_once_after_the_leaf) {
    ResetRecords();
    const void* addr = nullptr;
    void* baseTable = nullptr;
    void* leafTable = nullptr;
    {
        RealCtorThread t(0x10000);
        addr = &t;
        baseTable = g_systemStub_InitFields.vptrAtCall;
        leafTable = VptrOf(&t);
    }
    // The leaf's dtor body ran under the leaf table, then ~Thread re-stamped
    // the base table (retail's `LEA RAX,[0x180155f20]; MOV [RCX],RAX`) and ran
    // the teardown body exactly once.
    CHECK(g_vptrInDerivedDtorBody == leafTable);
    CHECK_EQ(g_systemStub_ReleaseFields.calls, 1);
    CHECK(g_systemStub_ReleaseFields.self == addr);
    CHECK(g_systemStub_ReleaseFields.vptrAtCall == baseTable);
    CHECK_EQ(g_systemStub_InitFields.calls, 1);
}
