// code/Game/Component/attendancecomponent_states.cpp
// The AttendanceComponent controller state machine.
//
// AttendanceComponent backs the buildings citizens VISIT rather than work at all day
// (school house, chapel, tavern seating, town hall gathering). It is NOT a
// ResourceProduceComponent subclass, so it does not share the RPC state table — it
// owns a private one, filled in retail by AttendanceDescription_Setup (0x14014a7e0)
// -> AttendanceDescription_helper_4630 (0x140044630) -> sub_28b0 / sub_29d0
// (retail table DAT_1401c81d0, count DAT_1401c9150, stride 0xf8 — the same shape
// controller_state_table.h models).
//
// The retail populator is exactly six calls, and it registers only TWO states:
//
//   state 2 (OPEN)    enter    0x140043c90  — register in the town attendance list,
//                                             mint a LocationWork, post it to the
//                                             citizen ledger, open the workplace
//                     trans38 0x140043c30  — PER-FRAME: drive the "full" mesh flag
//                     -> 3
//   state 3 (CLOSED)  enter    0x140043bd0  — unregister, send everyone home, close
//                                             the workplace
//                     -> 6, -> 4, -> 5
//
// Transitions carry no action handlers (retail passes 0 for every one), and states
// 4/5/6 are teardown targets with no body of their own — so they are deliberately
// NOT registered here, exactly as retail leaves them.
//
// Default state is 2 (component vtable slot 16 = `MOV EAX,2; RET` at 0x1400026c0)
// and the embedded node sits at +0x38 (slot 17 = `LEA RAX,[RCX+0x38]; RET` at
// 0x1400026b0) — NOT the ResourceProduce family's +0x28. AttendanceComponent_Ctor
// (0x140044580) confirms it: it writes the node vtable to +0x38 and the owning
// component back-pointer to +0x50 (= node + 0x18).

#include "Component/controller_state_table.h"
#include "Work/LocationWork.h"   // B.3.8 CB1a (Vtable)
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ENDGAME B.3.8 MI-A: the tag lives with the class (its controller member is
// ControllerLive::TableController<AttendanceCtrlTag>, AttendanceComponent.h).
#include "Component/AttendanceComponent.h"
using AttendanceDispatch = ::ControllerLive::Dispatcher<AttendanceCtrlTag>;

// Ported in attendancecomponent.cpp — state 3's enter drains the waiting queue.
extern void AttendanceComponent_ProcessQueue(void* self);

// ── Resolved engine helpers ───────────────────────────────────────────────────────
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised into
// the exe (ENDGAME B.3.2 step 2) — the town-list append, converted at its call
// site below to GameColl::AsArray<GameColl::Ptr8>(...).SetSizeFill(n, &fill).
// 0x1400f72e0 Work_Ctor25(work, ownerEntityIBP)                — base Work ctor.
// 0x140003050 FindComponent_Work(entity, outIBP)               — the citizen work ledger.
// 0x140002e10 FindComponent_WorkPlace(entity, outIBP)          — WorkPlaceComponent.
// 0x14012ec60 CitizenAI_impl_ec60(workComp, code, flag, workIBP) — post to the ledger.
// 0x14012fc00 WorkPlace SetAccepting(wpc, accepting).
// 0x140002f90 Entity_FindComponent_MeshInstance(entity, outIBP).
// 0x14012c3f0 MeshInstance set-state-flag(mesh, flagIdx, value).
typedef void* (*fn_WorkCtor25_t)(void* self, void* pEntityIBP);
typedef void  (*fn_FindComp_t)(uintptr_t entity, uintptr_t* outIBP);
typedef void  (*fn_PostWork_t)(uintptr_t workComp, uint32_t code, char flag, void* workIBP);
typedef void  (*fn_SetAccepting_t)(uintptr_t wpc, char accepting);
typedef void  (*fn_MeshSetFlag_t)(void* mesh, uint8_t flagIdx, char value);

static fn_WorkCtor25_t   s_workCtor25   = nullptr;
static fn_FindComp_t     s_findWork     = nullptr;
static fn_FindComp_t     s_findWpc      = nullptr;
static fn_PostWork_t     s_postWork     = nullptr;
static fn_SetAccepting_t s_setAccepting = nullptr;
static fn_FindComp_t     s_findMesh     = nullptr;
static fn_MeshSetFlag_t  s_meshSetFlag  = nullptr;
static uint32_t*         s_tidAttendanceDesc = nullptr;   // 0x1401c3f78
static uintptr_t         s_locationWorkVtable = 0;        // 0x1401616a0
static bool              s_resolved     = false;

bool LocationWork_ResolveFuncs();   // code/Game/Work/locationwork.cpp

static void AC_Resolve() {
    if (s_resolved) return;
    s_workCtor25   = reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_findWork     = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_findWpc      = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_postWork     = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_setAccepting = reinterpret_cast<fn_SetAccepting_t>(ResolveAddr(Module::Exe, 0x14012fc00));
    s_findMesh     = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002f90));
    s_meshSetFlag  = reinterpret_cast<fn_MeshSetFlag_t>(ResolveAddr(Module::Exe, 0x14012c3f0));
    s_tidAttendanceDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c3f78));
    s_locationWorkVtable = reinterpret_cast<uintptr_t>(LocationWork::Vtable());   // B.3.8: the compiler vtable (same bytes, B8-5)
    s_resolved = true;
}

// The town attendance registry: gameWorld+0xf0 is the town object, +0x60 its array
// base and +0x68 the element count. Both state 2's enter (append) and the component's
// vtable slot 7 (erase) operate on it, and helper_3db0 below walks it.
static uintptr_t AC_TownObject() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    return *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
}

// ── AttendanceDescription_helper_3db0 (0x140043db0) — "is every building of my kind
//    full?" ─────────────────────────────────────────────────────────────────────────
// Walks the town attendance registry looking at every OTHER component that shares this
// one's Description (comp+0x18). If ANY of them still has room — queue count (+0x30)
// below the description's capacity (desc+0x20) — the answer is 0 (not full). Only when
// every one is at capacity does it return 1.
//
// Note the scope: the flag is per-DESCRIPTION, not per-building, so a town with two
// schools shows both as "full" only once both are. That is retail's behaviour, and it
// is what makes the visual read as "no more room in town for this" rather than "this
// particular door is shut".
//
// ⚠ The desc deref after IsDerivedFrom is unguarded in retail and left unguarded here:
// the equality test on the line above already established that entry carries this exact
// description pointer, so the cast cannot fail.
extern "C" char AttendanceComponent_AllFull(void* selfP) {
    AC_Resolve();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    const uintptr_t town = AC_TownObject();
    if (!town) return 1;

    const uintptr_t base  = *reinterpret_cast<uintptr_t*>(town + 0x60);
    const int       count = *reinterpret_cast<int32_t*>(town + 0x68);
    const uintptr_t myDesc = *reinterpret_cast<uintptr_t*>(self + 0x18);

    for (int i = 0; i < count && base; ++i) {
        uintptr_t entry = *reinterpret_cast<uintptr_t*>(base + static_cast<uintptr_t>(i) * 8);
        if (!entry) continue;
        if (*reinterpret_cast<uintptr_t*>(entry + 0x18) != myDesc) continue;

        void* desc = *reinterpret_cast<void**>(entry + 0x18);
        if (s_tidAttendanceDesc && fn_IsDerivedFrom &&
            !fn_IsDerivedFrom(desc, *s_tidAttendanceDesc))
            desc = nullptr;
        const int capacity = *reinterpret_cast<int32_t*>(
            reinterpret_cast<uintptr_t>(desc) + 0x20);
        if (*reinterpret_cast<int32_t*>(entry + 0x30) < capacity)
            return 0;
    }
    return 1;
}

// ── state 2 trans38 — retail 0x140043c30 — PER-FRAME ──────────────────────────────
// Pushes the "all full" answer into the building's MeshInstance state flag 2, which is
// the closed/full visual. This is the only per-frame work the attendance controller
// does; there is no update handler on either state.
static char Attendance_State2Frame(void* selfP, void* /*sm*/, void* /*time*/) {
    AC_Resolve();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    if (!s_findMesh || !s_meshSetFlag) return 0;

    uintptr_t meshIbp = 0;
    s_findMesh(*reinterpret_cast<uintptr_t*>(self + 0x10), &meshIbp);

    const char full = AttendanceComponent_AllFull(selfP);
    // ⚠ Retail reads `*(longlong**)self` where `self` is the finder's RETURN value —
    // and the finder RETURNS ITS OUT-PARAM ADDRESS, so retail's deref is simply
    // "load meshIbp". We already hold meshIbp by value, so there is nothing left to
    // dereference: adding retail's `*` here handed 12c3f0 the component's VTABLE and
    // it faulted reading -1 (see reference_outparam_finder_recycled_ibp).
    s_meshSetFlag(reinterpret_cast<void*>(meshIbp), 2, full);
    return 0;
}

// ── state 2 enter — retail 0x140043c90 (Ghidra: "LocationWork_Ctor") ──────────────
// This is the OPEN transition and it is four distinct steps:
//   1. append this component to the town attendance registry (gw+0xf0 +0x60/+0x68) —
//      the list helper_3db0 above walks, and the list slot 7 erases from;
//   2. mint a 0x50-byte LocationWork bound to the owner entity;
//   3. post it to the entity's WorkComponent ledger via CitizenAI_impl_ec60(.., 1, 0, ..)
//      — this is what makes citizens actually walk here;
//   4. open the workplace (SetAccepting(wpc, 1)).
// Steps 2-4 are the same shape as the ResourceProduce state-13 enter; step 1 is what
// makes attendance different — the building has to be discoverable town-wide.
//
// ⚠ Ghidra's name is a misnomer of the same family as "HerbalistWork_Ctor" for
// HerbalistComponent's slot 18: this is a state ENTER handler that happens to build a
// LocationWork, not LocationWork's constructor (that is 0x1400efc60).
static char Attendance_State2Enter(void* selfP, void* /*sm*/) {
    AC_Resolve();
    LocationWork_ResolveFuncs();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfP);

    // 1. register in the town attendance list
    const uintptr_t town = AC_TownObject();
    if (town) {
        uintptr_t selfIbp = 0;
        if (fn_InternalBasePtr_Ctor) new (&selfIbp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(selfP));   // B.3.3 bridge
        // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
        const int32_t newCount = *reinterpret_cast<int32_t*>(town + 0x68) + 1;
        GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(town + 0x60))
            .SetSizeFill(newCount, &selfIbp);
    }

    // 2. mint the LocationWork
    void* work = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton)
        work = fn_MemAlloc(g_pMemAllocSingleton, 0x50, 8);
    if (work) {
        uintptr_t entity = *reinterpret_cast<uintptr_t*>(self + 0x10);
        if (s_workCtor25) s_workCtor25(work, &entity);
        if (s_locationWorkVtable)
            *reinterpret_cast<uintptr_t*>(work) = s_locationWorkVtable;
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(work) + 0x48) = 0;
    }
    uintptr_t workIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&workIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge

    // 3. post it to the citizen work ledger
    if (s_findWork && s_postWork) {
        uintptr_t workCompIbp = 0;
        s_findWork(*reinterpret_cast<uintptr_t*>(self + 0x10), &workCompIbp);
        s_postWork(workCompIbp, 1, '\0', &workIbp);
    }

    // 4. open the workplace
    if (s_findWpc && s_setAccepting) {
        uintptr_t wpcIbp = 0;
        s_findWpc(*reinterpret_cast<uintptr_t*>(self + 0x10), &wpcIbp);
        s_setAccepting(wpcIbp, '\x01');
    }

    return 0;
}

// ── state 3 enter — retail 0x140043bd0 ────────────────────────────────────────────
// The CLOSED transition, the exact mirror of state 2:
//   1. component vtable slot 7 (+0x38) — erase from the town attendance registry;
//   2. AttendanceComponent_ProcessQueue — send every queued citizen home;
//   3. SetAccepting(wpc, 0) — close the workplace.
// Step 1 goes through the vtable rather than calling the ported body directly, because
// that is what retail does and because slot 7 is installed in the same wave — if it
// ever regressed to the inert stub, a direct call here would hide it.
static char Attendance_State3Enter(void* selfP, void* /*sm*/) {
    AC_Resolve();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfP);

    void** vt = *reinterpret_cast<void***>(selfP);
    if (vt && vt[7]) reinterpret_cast<void (*)(void*)>(vt[7])(selfP);

    AttendanceComponent_ProcessQueue(selfP);

    if (s_findWpc && s_setAccepting) {
        uintptr_t wpcIbp = 0;
        s_findWpc(*reinterpret_cast<uintptr_t*>(self + 0x10), &wpcIbp);
        s_setAccepting(wpcIbp, '\0');
    }
    return 0;
}

// ── the table — replicates AttendanceDescription_helper_4630 (0x140044630) ────────
extern "C" void AttendanceCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = AttendanceDispatch::table;
    if (t.count != 0) return;   // idempotent, like the retail Setup guard

    // sub_28b0(2, LocationWork_Ctor, .., .., sub_3c30) — enter + per-frame, no update.
    TableRegisterState(t, 2, reinterpret_cast<void*>(&Attendance_State2Enter),
                       nullptr, reinterpret_cast<void*>(&Attendance_State2Frame), nullptr);
    TableAddTransition(t, 3, nullptr);          // sub_29d0(3)

    // sub_28b0(3, helper_3bd0, .., .., 0) — enter only.
    TableRegisterState(t, 3, reinterpret_cast<void*>(&Attendance_State3Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);          // sub_29d0(6)
    TableAddTransition(t, 4, nullptr);          // sub_29d0(4)
    TableAddTransition(t, 5, nullptr);          // sub_29d0(5)
}
