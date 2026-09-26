// code/Game/Component/attendancecomponent.cpp
// Clean reimplementation of AttendanceComponent::Update from Application-x64.exe.
//
// AttendanceComponent manages a queue of citizens waiting to attend a building
// (e.g., church, tavern, school). When the building is idle (not busy), the
// component processes the attendance queue.
//
// Update logic (0x140044370, 22 instructions):
//   1. Check if queue count (+0x30 as int) is nonzero; early-out if empty
//   2. Get the entity's building component via FUN_140002e10
//   3. Check if building is idle (building+0x60 == 0)
//   4. If idle, call FUN_140043f60 to process the attendance queue
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/attendancecomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeNameById (pre-fault report)
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140002e10(entityPtr, outBuildingPtr) -- get building component from entity
typedef void (*fn_GetBuildingComponent_t)(void* entity, void* outPtr);
static fn_GetBuildingComponent_t s_fn_GetBuildingComponent = nullptr;

// FUN_140043f60(self) -- process attendance queue
typedef void (*fn_ProcessAttendanceQueue_t)(void* self);
static fn_ProcessAttendanceQueue_t s_fn_ProcessAttendanceQueue = nullptr;

static bool s_initialized = false;

static void AttendanceComponent_ReportMissingWorkplace(AttendanceComponent* self, void* entity,
                                                       int32_t queueCount);

// ============================================================
// AttendanceComponent::Update (vtable slot [9], 0x140044370)
// ============================================================
void AttendanceComponent_Update(AttendanceComponent* self) {
    // Resolve the helpers on first use. AttendanceComponent_InitFuncs had NO caller
    // anywhere (found 2026-09-03 by the BANISHED_STRESS town): every Update ran with
    // s_fn_GetBuildingComponent == null, so buildingPtr stayed 0 and the +0x60 read
    // below faulted the first time any attendance queue was non-empty. It never showed
    // before because no run had ever STAFFED a chapel/school/hospital, and an unstaffed
    // one never gets a queue, so the early-out above hid it. Idempotent after the first call.
    AttendanceComponent_InitFuncs();
    uintptr_t selfAddr = (uintptr_t)self;

    // Check if queue count at +0x30 (as int32) is nonzero
    int32_t queueCount = *(int32_t*)(selfAddr + 0x30);
    if (queueCount == 0)
        return;

    // Get entity pointer
    void* entity = *(void**)(selfAddr + 0x10);

    // Get building component from entity
    int64_t buildingPtr = 0;
    if (s_fn_GetBuildingComponent)
        s_fn_GetBuildingComponent(entity, &buildingPtr);

    // DIAGNOSTIC ONLY (the deref below stays faithful): retail relies on every attendance
    // building carrying a WorkPlaceComponent, so a null here means something upstream
    // handed this component an entity without one. Say WHICH entity before faulting,
    // because the crash log alone cannot (first seen in the BANISHED_STRESS town).
    if (buildingPtr == 0) {
        AttendanceComponent_ReportMissingWorkplace(self, entity, queueCount);
    }

    // Check if building is idle (building+0x60 == 0)
    int32_t busyState = *(int32_t*)(buildingPtr + 0x60);
    if (busyState == 0) {
        // Building is idle -- process the attendance queue
        if (s_fn_ProcessAttendanceQueue)
            s_fn_ProcessAttendanceQueue(self);
    }
}

// ============================================================
// Reimpl helpers for the attendance functions
// ============================================================
// DWORD_1401c80e0 — job-host component type ID
static uint32_t* s_pTId_JobHost = nullptr;

// Pre-fault report for AttendanceComponent_Update (see the call site).
static void AttendanceComponent_ReportMissingWorkplace(AttendanceComponent* self, void* entity,
                                                       int32_t queueCount) {
    uint8_t* e = reinterpret_cast<uint8_t*>(entity);
    fprintf(stderr, "[AttendanceComponent::Update] self=%p queue(+0x30)=%d owner entity=%p has NO "
            "WorkPlaceComponent (0x140002e10 returned null) -- about to fault faithfully\n",
            (void*)self, queueCount, entity);
    if (e) {
        const uint32_t flags = *reinterpret_cast<uint32_t*>(e + 0x40);
        const int32_t  n     = *reinterpret_cast<int32_t*>(e + 0x38);
        int64_t* comps       = *reinterpret_cast<int64_t**>(e + 0x30);
        fprintf(stderr, "[AttendanceComponent::Update]   entity flags=0x%x components=%d:", flags, n);
        for (int i = 0; comps && i < n && i < 32; ++i) {
            int64_t c = comps[i];
            if (!c) { fprintf(stderr, " <null>"); continue; }
            void** vt = *reinterpret_cast<void***>(c);
            const uint32_t tid = (vt && vt[2]) ? reinterpret_cast<uint32_t(*)(void*)>(vt[2])(reinterpret_cast<void*>(c)) : 0;
            const char* nm = ::Object::GetTypeNameById(tid);
            const bool isHost = s_pTId_JobHost && tid == *s_pTId_JobHost;
            fprintf(stderr, " %s(%u%s)", (nm && nm[0]) ? nm : "?", tid,
                    isHost ? (fn_IsDerivedFrom && fn_IsDerivedFrom(reinterpret_cast<void*>(c), tid)
                              ? ",HOST,derived" : ",HOST,NOT-derived") : "");
        }
        fprintf(stderr, "\n[AttendanceComponent::Update]   jobHost tid(1401c80e0)=%u finder=%p\n",
                s_pTId_JobHost ? *s_pTId_JobHost : 0, (void*)s_fn_GetBuildingComponent);
    }
    fflush(stderr);
}

// Queue-drain dispatch helpers
typedef void (*fn_AC_115a50_t)(void* worker, char p2);   // 0x140115a50
typedef void (*fn_AC_59b0_t)(void* worker);              // 0x1401159b0
typedef void (*fn_AC_1650_t)(void* worker);              // 0x140111650
static fn_AC_115a50_t s_fn_AC_115a50 = nullptr;
static fn_AC_59b0_t   s_fn_AC_59b0 = nullptr;
static fn_AC_1650_t   s_fn_AC_1650 = nullptr;

// Callees of the two newly-reimplemented worker-release helpers (Entity_impl_1650/59b0):
typedef void  (*fn_AC_002cc0_t)(int64_t* listBase, void* ibp);          // 0x140002cc0
typedef void* (*fn_AC_FindMesh_t)(int64_t entity, void* outIBP);        // 0x140002f90
typedef void  (*fn_AC_MeshSetFlag_t)(void* mesh, uint8_t flagIdx, char value); // 0x14012c3f0
static fn_AC_002cc0_t      s_fn_AC_002cc0 = nullptr;
static fn_AC_FindMesh_t    s_fn_AC_FindMesh = nullptr;
static fn_AC_MeshSetFlag_t s_fn_AC_MeshSetFlag = nullptr;
// CitizenComponent::TryFindPartner (0x140111840) — registered by Batch A cascade round.
typedef char (*fn_AC_TryEnrolInSchool_t)(void* citizen, int64_t* exclIBP);
static fn_AC_TryEnrolInSchool_t s_fn_AC_TryEnrolInSchool = nullptr;

// Slot-3 (Shutdown/Serialize) helpers — see AttendanceComponent_SerializeSlot below.
// Component_Serialize 0x1401189b0; the queue
// array grow 0x1400032e0; and DWORD_1401c7158, the CitizenComponent type id the
// per-entry filter tests against.
typedef void  (*fn_Component_Serialize_t)(void* self, void* stream);
// 0x1400032e0 was Collection::Array<void*>::SetSize(n) monomorphised into the
// exe (ENDGAME B.3.2 step 2) — converted at its call site below.
static fn_Component_Serialize_t     s_fn_Component_Serialize     = nullptr;
static uint32_t*                    s_pTId_CitizenComponent      = nullptr;  // 0x1401c7158

static bool s_acHelpersResolved = false;
static void EnsureAcHelpers() {
    if (s_acHelpersResolved) return;
    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_pTId_CitizenComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));
    s_pTId_JobHost = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c80e0));
    s_fn_AC_115a50 = reinterpret_cast<fn_AC_115a50_t>(ResolveAddr(Module::Exe, 0x140115a50));
    s_fn_AC_59b0   = reinterpret_cast<fn_AC_59b0_t>(ResolveAddr(Module::Exe, 0x1401159b0));
    s_fn_AC_1650   = reinterpret_cast<fn_AC_1650_t>(ResolveAddr(Module::Exe, 0x140111650));
    s_fn_AC_002cc0     = reinterpret_cast<fn_AC_002cc0_t>(ResolveAddr(Module::Exe, 0x140002cc0));
    s_fn_AC_FindMesh   = reinterpret_cast<fn_AC_FindMesh_t>(ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_AC_MeshSetFlag= reinterpret_cast<fn_AC_MeshSetFlag_t>(ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_AC_TryEnrolInSchool = reinterpret_cast<fn_AC_TryEnrolInSchool_t>(ResolveAddr(Module::Exe, 0x140111840));
    s_acHelpersResolved = true;
}

// ============================================================
// Reimpl of FUN_140115a50 (0x140115a50):
// release the citizen's partner-link slot (+0x208): detach from the partner entity's
// worker list (002cc0), reset its MeshInstance flag 2 off (12c3f0), null the slot; then
// if param_2 set, immediately re-roll for a new partner (TryFindPartner) and clear the
// slot again on failure. Closure-safe after the cascade registered TryFindPartner.
void AttendanceComponent_ReleasePartnerSlot(void* workerP, char reroll) {
    EnsureAcHelpers();
    uint8_t* self = reinterpret_cast<uint8_t*>(workerP);
    int64_t* slot = reinterpret_cast<int64_t*>(self + 0x208);
    if (*slot == 0) return;

    int64_t selfIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&selfIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(workerP));   // B.3.3 bridge
    int64_t linked = *slot;
    if (s_fn_AC_002cc0) s_fn_AC_002cc0(reinterpret_cast<int64_t*>(linked + 0x28), &selfIbp);

    int64_t meshIbp = 0;
    void* mesh = s_fn_AC_FindMesh
                     ? s_fn_AC_FindMesh(*reinterpret_cast<int64_t*>(linked + 0x10), &meshIbp)
                     : nullptr;
    if (s_fn_AC_MeshSetFlag && mesh)
        s_fn_AC_MeshSetFlag(*reinterpret_cast<void**>(mesh), 2, '\0');

    int64_t nullIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *slot = nullIbp;

    if (reroll != '\0') {
        int64_t exclIbp = 0;
        if (fn_InternalBasePtr_Ctor) new (&exclIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        char found = s_fn_AC_TryEnrolInSchool ? s_fn_AC_TryEnrolInSchool(workerP, &exclIbp) : 0;
        if (found == '\0') {
            int64_t z = 0;
            if (fn_InternalBasePtr_Ctor) new (&z) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *slot = z;
        }
    }
}

// ============================================================
// Reimpl of Entity_impl_1650 (0x140111650) and Entity_impl_59b0 (0x1401159b0):
// release a queued-worker link slot. Both are identical apart from the slot offset
// (1650 -> +0x230, 59b0 -> +0x228). When the slot holds a linked entity: detach this
// worker from the entity's worker list (FUN_140002cc0), reset that entity's
// MeshInstance state flag 2 to off (FUN_14012c3f0), then null the slot.
static void AttendanceComponent_ReleaseLinkSlot(void* workerP, int slotOff) {
    EnsureAcHelpers();
    uint8_t* self = reinterpret_cast<uint8_t*>(workerP);
    int64_t* slot = reinterpret_cast<int64_t*>(self + slotOff);
    if (*slot == 0) return;

    int64_t selfIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&selfIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(workerP));   // B.3.3 bridge
    int64_t linked = *slot;
    if (s_fn_AC_002cc0) s_fn_AC_002cc0(reinterpret_cast<int64_t*>(linked + 0x28), &selfIbp);

    int64_t meshIbp = 0;
    void* mesh = s_fn_AC_FindMesh
                     ? s_fn_AC_FindMesh(*reinterpret_cast<int64_t*>(linked + 0x10), &meshIbp)
                     : nullptr;
    if (s_fn_AC_MeshSetFlag && mesh)
        s_fn_AC_MeshSetFlag(*reinterpret_cast<void**>(mesh), 2, '\0');

    int64_t nullIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *slot = nullIbp;
}

void AttendanceComponent_ReleaseLink1650(void* workerP) {
    AttendanceComponent_ReleaseLinkSlot(workerP, 0x230);
}
void AttendanceComponent_ReleaseLink59b0(void* workerP) {
    AttendanceComponent_ReleaseLinkSlot(workerP, 0x228);
}

// ============================================================
// Reimpl of FUN_140002e10 (find the job-host component (type DWORD_1401c80e0)
//   on an entity and return it wrapped in an InternalBasePtr via retbuf)
// ============================================================
void AttendanceComponent_GetJobHostComponent(void* entity, void* outPtr) {
    EnsureAcHelpers();
    void* found = nullptr;
    if (s_pTId_JobHost) {
        int64_t host = (int64_t)entity;
        int count = *(int*)(host + 0x38);
        int64_t arr = *(int64_t*)(host + 0x30);
        for (int i = 0; i < count; i++) {
            int64_t* slot = (int64_t*)(arr + (int64_t)i * 8);
            void* comp = (void*)*slot;
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == *s_pTId_JobHost) {
                void* c = (void*)*slot;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_pTId_JobHost)) found = c;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) new (outPtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
}

// ============================================================
// Reimpl of FUN_140043f60 (AttendanceComponent_sub_3f60:
//   drain the attendance queue, dispatching each worker by which list it is on)
// ============================================================
// Queue InternalBasePtr array at self+0x28 (count self+0x30). For each worker the
// helpers below remove it, so the loop re-reads the count each pass. The dispatch
// compares the queued IBP against three worker fields (+0x208/+0x228/+0x230).
void AttendanceComponent_ProcessQueue(void* selfP) {
    EnsureAcHelpers();
    uintptr_t self = (uintptr_t)selfP;
    while (*(int*)(self + 0x30) > 0) {
        int64_t queuedIbp = 0;
        if (fn_InternalBasePtr_Ctor) new (&queuedIbp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(selfP));   // B.3.3 bridge
        int64_t* slot = *(int64_t**)(self + 0x28);
        int64_t worker = *slot;
        if (*(int64_t*)(worker + 0x208) == queuedIbp) {
            if (s_fn_AC_115a50) s_fn_AC_115a50((void*)worker, '\x01');
        } else {
            if (*(int64_t*)(worker + 0x228) == queuedIbp) {
                if (s_fn_AC_59b0) s_fn_AC_59b0((void*)worker);
            } else {
                if (*(int64_t*)(worker + 0x230) == queuedIbp) {
                    if (s_fn_AC_1650) s_fn_AC_1650((void*)worker);
                }
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AttendanceComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_GetBuildingComponent = reinterpret_cast<fn_GetBuildingComponent_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    s_fn_ProcessAttendanceQueue = reinterpret_cast<fn_ProcessAttendanceQueue_t>(
        ResolveAddr(Module::Exe, 0x140043f60));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AttendanceComponent] Funcs resolved: GetBuilding=%p ProcessQueue=%p\n",
           (void*)s_fn_GetBuildingComponent,
           (void*)s_fn_ProcessAttendanceQueue);
#endif
}

// ============================================================
// Vtable slots 13/14 — the panel COMMAND pair
// ============================================================
// AttendanceComponent is NOT a ResourceProduceComponent subclass, so it shares nothing
// with the base command pair. Byte-verified from vtable_AttendanceComponent
// (0x140158ce0): slot 13 = 0x140043f10, slot 14 = 0x140043e80.
//
// Both bodies delegate to the entity's WorkPlaceComponent rather than holding state of
// their own: the attendance building's "work" checkbox IS the workplace's accepting
// flag (WPC+0x50). That is why this class overrides instead of inheriting — and why
// both bodies start by re-finding the WPC off the owner entity (self+0x10) via
// 0x140002e10, already resolved above as s_fn_GetBuildingComponent.

extern void WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting);  // 0x14012fc00

// ── slot 13 — retail 0x140043f10 — availability mask ──────────────────────────────
// Retail body: find the WPC, then `return -(*(char*)(wpc+0x50) != 0) & 2 | 1;`
// i.e. `accepting ? 3 : 1` — bit 0 "command offered", bit 1 "currently ON". It never
// returns 0, so the checkbox is never greyed by this slot.
//
// ⚠ Retail takes only `self`; the command id the caller passes in RDX is ignored. We
// declare the parameter so the vtable call is ABI-compatible, and ignore it as retail does.
//
// ⚠ The `wpc+0x50` deref is UNGUARDED in retail and left unguarded here: retail relies on
// an attendance building always carrying a WorkPlaceComponent. If this ever faults, the
// fault is an entity built without a WPC upstream — do not paper over it here.
extern "C" uint8_t AttendanceComponent_CommandMask(void* selfP, int /*cmd*/) {
    AttendanceComponent_InitFuncs();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    if (!s_fn_GetBuildingComponent) return 0;
    int64_t wpcIbp = 0;
    s_fn_GetBuildingComponent(*reinterpret_cast<void**>(self + 0x10), &wpcIbp);
    return *reinterpret_cast<char*>(wpcIbp + 0x50) ? 3 : 1;
}

// ── slot 14 — retail 0x140043e80 — flip the workplace's accepting flag ────────────
// Retail body:
//     wpc = FindWorkPlaceComponent(self+0x10)
//     cur = *(char*)(wpc+0x50)
//     WorkPlaceComponent_SetAccepting(wpc, cur == 0)      // 0x14012fc00
//     if (*(char*)(wpc+0x50) == 0) AttendanceComponent_ProcessQueue(self)
//
// The re-read after SetAccepting is load-bearing, not redundant: SetAccepting is a large
// body (mesh-visual toggle, status-icon bit, and — when turning OFF — a loop that releases
// every seated worker), and the queue drain must observe the flag it actually left behind.
// Draining on the OFF edge is what sends the students//workers home; on the ON edge there
// is nothing to drain.
//
// ⚠ Retail ignores the command id here too.
extern "C" void AttendanceComponent_Command(void* selfP, int /*cmd*/) {
    AttendanceComponent_InitFuncs();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    if (!s_fn_GetBuildingComponent) return;
    int64_t wpcIbp = 0;
    s_fn_GetBuildingComponent(*reinterpret_cast<void**>(self + 0x10), &wpcIbp);
    const char cur = *reinterpret_cast<char*>(wpcIbp + 0x50);
    WorkPlaceComponent_SetAccepting(wpcIbp, cur == 0);
    if (*reinterpret_cast<char*>(wpcIbp + 0x50) == 0)
        AttendanceComponent_ProcessQueue(selfP);
}

// ============================================================
// Vtable slot 7 — retail 0x1400443d0 — leave the town attendance registry
// ============================================================
// Ghidra calls this "AttendanceComponent_OnCreate". It is slot 7, which on this
// component family is OnRemove, not OnAdded — the usual off-by-one mislabel (see
// reference_ghidra_oncreate_onremove_mislabel). The body is a single erase-by-value
// from the town attendance list at gameWorld+0xf0 +0x60, the list that state 2's enter
// appends to and that AttendanceComponent_AllFull walks. Without it a demolished
// school stays in the list forever and every surviving school reads as "full".
//
// It is also called directly by state 3's enter (through the vtable), which is the
// "close for the night" path — so this is not only teardown.
extern "C" void AttendanceComponent_Unregister(void* selfP) {
    EnsureAcHelpers();
    if (!s_fn_AC_002cc0 || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    const uintptr_t town = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!town) return;
    int64_t selfIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&selfIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(selfP));   // B.3.3 bridge
    s_fn_AC_002cc0(reinterpret_cast<int64_t*>(town + 0x60), &selfIbp);
}

// ============================================================
// Vtable slot 3 — retail 0x140044410 — Shutdown / Serialize
// ============================================================
// Ghidra labels it "AttendanceComponent_Controller_Slot13"; the vtable BYTES at
// 0x140158ce0 put it at index 3, which is Shutdown across this whole family.
//
//   Component_Serialize(self, stream)                       0x1401189b0
//   { Serializer(stream, 0) }                               // empty block, subclass compat
//   Serializer s(stream, 0)
//   int n = self[+0x30];  s.Serialize(&n)                   // the waiting-queue count
//   if (s[+8] == 0) GrowArray(self+0x28, n)                 // READING: size the array first
//   for (i in 0..n) { InternalBasePtr::Serialize(&q[i], s)
//                     if (q[i] && !IsDerivedFrom(q[i], CitizenComponent)) q[i] = 0 }
//
// The IsDerivedFrom filter is a load-bearing part of load, not a sanity check: a saved
// queue entry whose citizen no longer exists deserializes to some other object, and
// retail nulls the slot rather than letting ProcessQueue dispatch on it.
extern "C" void AttendanceComponent_SerializeSlot(void* selfP, void* stream) {
    AttendanceComponent_InitFuncs();
    EnsureAcHelpers();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfP);

    if (s_fn_Component_Serialize) s_fn_Component_Serialize(selfP, stream);

    // Empty block — retail opens and closes a Serializer with nothing in it.
    alignas(8) uint8_t empty[24];
    ::new (static_cast<void*>(empty)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(empty)->~Serializer();                                       // B.3.4 bridge

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int32_t count = *reinterpret_cast<int32_t*>(self + 0x30);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge

    // The queue array has to be grown to the deserialized count before the
    // per-entry loop writes into it.
    // ⚠ IO::Serializer +0x08 is m_encoding (a POINTER, never 0) — testing it as
    // the loading flag leaves this load-only branch dead on BOTH legs, so the
    // array is never grown and the loop below silently skips reading the IBPs
    // the save wrote -> stream desync. The flag is on the STREAM (+0x08).
    // Same defect as WorkPlaceComponent (21b3b33); see
    // reference_serializer_plus8_is_encoding_not_loading_flag.
    const uintptr_t acStream = *reinterpret_cast<uintptr_t*>(ser);
    if (acStream && static_cast<int>(reinterpret_cast<IO::Serializer*>(ser)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
        GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(self + 0x28)).SetSize(count);
    }

    int64_t* q = *reinterpret_cast<int64_t**>(self + 0x28);
    for (int32_t i = 0; i < count && q; ++i, ++q) {
        reinterpret_cast<Object::InternalBasePtr*>(q)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
        if (*q == 0) continue;
        // Value check, not just pointer: a registered-but-unfilled slot reads 0 and
        // IsDerivedFrom(x, 0) would drop every attendee.
        if (s_pTId_CitizenComponent && *s_pTId_CitizenComponent && fn_IsDerivedFrom &&
            !fn_IsDerivedFrom(reinterpret_cast<void*>(*q), *s_pTId_CitizenComponent))
            *q = 0;
    }

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();                                       // B.3.4 bridge
}

// ============================================================================
// ENDGAME B.3.8 MI-A — AttendanceComponent as a REAL class (AttendanceComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES (a 48-slot table re-filled at every
// create, slots 3/7/9/13/14/18-21 re-installed from PopulateAndWireSlots), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<AttendanceCtrlTag> table stamped into raw bytes at +0x38.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/AttendanceComponent.h"

// The controller drives the OPEN (2) / CLOSED (3) states (attendancecomponent_states.cpp);
// slots 16/17 are what let the StateMachineComponent link the member. Default state 2
// (0x1400026c0), member at +0x38 (0x1400026b0) — NOT the ResourceProduce family's
// +0x28. The state table is built ONCE at static-init time, as retail's static
// initializer does (B38 D6/D7).
//
// The tree's slots 18-21 (ControllerLive_ScalarDeletingDtor, the description type
// info / type id, `return 1`) were AttendanceDescription's table 0x140158d70 read past
// retail's 18-slot end (B38 D3.5); no dispatch past slot 17 exists in the tree. Gone.
extern "C" void AttendanceCtrl_PopulateTable();
static const bool s_attendanceStateTableBuilt = (AttendanceCtrl_PopulateTable(), true);

// 0x140044580. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// +0x28 = +0x30 = 0; `_controller(this)` is the member's back-pointer at +0x50; then
// `_flags |= 0xa` (0x8a).
AttendanceComponent::AttendanceComponent()
    : field28(nullptr), field30(nullptr), _controller(this) {
    OrFlagsInCtor(0xa);
}

// [0] 0x140002860: ~AttendanceComponent (0x140044510), MemFree on &1.
Object::Base* AttendanceComponent::DeletingDtor(unsigned flags) {
    this->~AttendanceComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x140044510: the waiting queue (+0x28 data, +0x30 count/capacity) is freed and
// zeroed; the member's and the bases' dtors (restamps only) and ~Internal are the
// compiler's chain.
AttendanceComponent::~AttendanceComponent() {
    Object::detail::InternalMemFree(field28);
    field28 = nullptr;
    field30 = nullptr;
}

// create 0x140002670: MemAlloc(0x58, 8), then the ctor.
void* AttendanceComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AttendanceComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AttendanceComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* AttendanceComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AttendanceComponent());
}

// [3] 0x140044410.
void AttendanceComponent::Persist(void* stream) { AttendanceComponent_SerializeSlot(this, stream); }

// [7] 0x1400443d0 — leave the town attendance registry (state 3's enter also calls
// it through the vtable).
void AttendanceComponent::OnRemove() { AttendanceComponent_Unregister(this); }

// [9] 0x140044370 — the elapsed-time argument is not read by retail's body.
void AttendanceComponent::Update(void* /*time*/) { AttendanceComponent_Update(this); }

// [13] 0x140043f10 — `accepting ? 3 : 1`; the command id is ignored, as in retail.
uint32_t AttendanceComponent::CommandMask(int command) {
    return AttendanceComponent_CommandMask(this, command);
}

// [14] 0x140043e80 — flip the workplace's accepting flag; drain on the OFF edge.
void AttendanceComponent::Command(int command) { AttendanceComponent_Command(this, command); }

DEFINE_EXE_VTABLE(AttendanceComponent, "_ZTV19AttendanceComponent")
DEFINE_EXE_TYPE(AttendanceComponent, "ControllerComponent")
