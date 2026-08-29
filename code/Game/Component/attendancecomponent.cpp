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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Object::InternalBasePtr::CheckPtr -- dispatch slot at [0x140152ab0]
typedef void (*fn_CheckPtr_t)(void* internalBasePtrAddr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// FUN_140002e10(entityPtr, outBuildingPtr) -- get building component from entity
typedef void (*fn_GetBuildingComponent_t)(void* entity, void* outPtr);
static fn_GetBuildingComponent_t s_fn_GetBuildingComponent = nullptr;

// FUN_140043f60(self) -- process attendance queue
typedef void (*fn_ProcessAttendanceQueue_t)(void* self);
static fn_ProcessAttendanceQueue_t s_fn_ProcessAttendanceQueue = nullptr;

static bool s_initialized = false;

// ============================================================
// AttendanceComponent::Update (vtable slot [9], 0x140044370)
// ============================================================
void AttendanceComponent_Update(AttendanceComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Check if queue count at +0x30 (as int32) is nonzero
    int32_t queueCount = *(int32_t*)(selfAddr + 0x30);
    if (queueCount == 0)
        return;

    // CheckPtr on owner smart pointer at +0x10
    if (s_fn_CheckPtr)
        s_fn_CheckPtr((void*)(selfAddr + 0x10));

    // Get entity pointer
    void* entity = *(void**)(selfAddr + 0x10);

    // Get building component from entity
    int64_t buildingPtr = 0;
    if (s_fn_GetBuildingComponent)
        s_fn_GetBuildingComponent(entity, &buildingPtr);

    // CheckPtr on returned building pointer
    if (s_fn_CheckPtr)
        s_fn_CheckPtr(&buildingPtr);

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
// Component_Serialize 0x1401189b0; the three IO::Serializer dispatch slots; the queue
// array grow 0x1400032e0; and DWORD_1401c7158, the CitizenComponent type id the
// per-entry filter tests against.
typedef void  (*fn_Component_Serialize_t)(void* self, void* stream);
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*fn_Serializer_Dtor_t)(void* self);
typedef void  (*fn_Serializer_SerializeInt_t)(void* self, void* field, const char* name);
typedef void  (*fn_AC_ArrayGrow_t)(void* array, int count);
static fn_Component_Serialize_t     s_fn_Component_Serialize     = nullptr;
static fn_Serializer_Ctor_t         s_fn_Serializer_Ctor         = nullptr;
static fn_Serializer_Dtor_t         s_fn_Serializer_Dtor         = nullptr;
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;
static fn_AC_ArrayGrow_t            s_fn_AC_ArrayGrow            = nullptr;
static uint32_t*                    s_pTId_CitizenComponent      = nullptr;  // 0x1401c7158

static bool s_acHelpersResolved = false;
static void EnsureAcHelpers() {
    if (s_acHelpersResolved) return;
    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_AC_ArrayGrow = reinterpret_cast<fn_AC_ArrayGrow_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));
    s_pTId_CitizenComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152ad8))
        s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152ad0))
        s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152ac8))
        s_fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(s);
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
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfIbp, workerP);
    if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
    int64_t linked = *slot;
    if (s_fn_AC_002cc0) s_fn_AC_002cc0(reinterpret_cast<int64_t*>(linked + 0x28), &selfIbp);

    if (s_fn_CheckPtr) s_fn_CheckPtr(reinterpret_cast<void*>(linked + 0x10));
    int64_t meshIbp = 0;
    void* mesh = s_fn_AC_FindMesh
                     ? s_fn_AC_FindMesh(*reinterpret_cast<int64_t*>(linked + 0x10), &meshIbp)
                     : nullptr;
    if (s_fn_CheckPtr) s_fn_CheckPtr(mesh);
    if (s_fn_AC_MeshSetFlag && mesh)
        s_fn_AC_MeshSetFlag(*reinterpret_cast<void**>(mesh), 2, '\0');

    int64_t nullIbp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIbp, nullptr);
    *slot = nullIbp;

    if (reroll != '\0') {
        int64_t exclIbp = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&exclIbp, nullptr);
        char found = s_fn_AC_TryEnrolInSchool ? s_fn_AC_TryEnrolInSchool(workerP, &exclIbp) : 0;
        if (found == '\0') {
            int64_t z = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&z, nullptr);
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
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfIbp, workerP);
    if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
    int64_t linked = *slot;
    if (s_fn_AC_002cc0) s_fn_AC_002cc0(reinterpret_cast<int64_t*>(linked + 0x28), &selfIbp);

    if (s_fn_CheckPtr) s_fn_CheckPtr(reinterpret_cast<void*>(linked + 0x10));
    int64_t meshIbp = 0;
    void* mesh = s_fn_AC_FindMesh
                     ? s_fn_AC_FindMesh(*reinterpret_cast<int64_t*>(linked + 0x10), &meshIbp)
                     : nullptr;
    if (s_fn_CheckPtr) s_fn_CheckPtr(mesh);
    if (s_fn_AC_MeshSetFlag && mesh)
        s_fn_AC_MeshSetFlag(*reinterpret_cast<void**>(mesh), 2, '\0');

    int64_t nullIbp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIbp, nullptr);
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
            if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
            void* comp = (void*)*slot;
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == *s_pTId_JobHost) {
                if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
                void* c = (void*)*slot;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_pTId_JobHost)) found = c;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outPtr, found);
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
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&queuedIbp, selfP);
        int64_t* slot = *(int64_t**)(self + 0x28);
        if (s_fn_CheckPtr) s_fn_CheckPtr(slot);
        int64_t worker = *slot;
        if (s_fn_CheckPtr) s_fn_CheckPtr(&queuedIbp);
        if (*(int64_t*)(worker + 0x208) == queuedIbp) {
            if (s_fn_AC_115a50) s_fn_AC_115a50((void*)worker, '\x01');
        } else {
            if (s_fn_CheckPtr) s_fn_CheckPtr(&queuedIbp);
            if (*(int64_t*)(worker + 0x228) == queuedIbp) {
                if (s_fn_AC_59b0) s_fn_AC_59b0((void*)worker);
            } else {
                if (s_fn_CheckPtr) s_fn_CheckPtr(&queuedIbp);
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

    // CheckPtr -- indirect call through dispatch slot at [0x140152ab0]
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_GetBuildingComponent = reinterpret_cast<fn_GetBuildingComponent_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    s_fn_ProcessAttendanceQueue = reinterpret_cast<fn_ProcessAttendanceQueue_t>(
        ResolveAddr(Module::Exe, 0x140043f60));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AttendanceComponent] Funcs resolved: CheckPtr=%p GetBuilding=%p ProcessQueue=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_GetBuildingComponent,
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
    if (s_fn_CheckPtr) s_fn_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    int64_t wpcIbp = 0;
    s_fn_GetBuildingComponent(*reinterpret_cast<void**>(self + 0x10), &wpcIbp);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&wpcIbp);
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
    if (s_fn_CheckPtr) s_fn_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    int64_t wpcIbp = 0;
    s_fn_GetBuildingComponent(*reinterpret_cast<void**>(self + 0x10), &wpcIbp);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&wpcIbp);
    const char cur = *reinterpret_cast<char*>(wpcIbp + 0x50);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&wpcIbp);
    WorkPlaceComponent_SetAccepting(wpcIbp, cur == 0);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&wpcIbp);
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
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&selfIbp, selfP);
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

    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) return;

    // Empty block — retail opens and closes a Serializer with nothing in it.
    uint8_t empty[24];
    s_fn_Serializer_Ctor(empty, stream, 0);
    s_fn_Serializer_Dtor(empty);

    uint8_t ser[24];
    s_fn_Serializer_Ctor(ser, stream, 0);

    int32_t count = *reinterpret_cast<int32_t*>(self + 0x30);
    if (s_fn_Serializer_SerializeInt) s_fn_Serializer_SerializeInt(ser, &count, nullptr);

    // The queue array has to be grown to the deserialized count before the
    // per-entry loop writes into it.
    // ⚠ IO::Serializer +0x08 is m_encoding (a POINTER, never 0) — testing it as
    // the loading flag leaves this load-only branch dead on BOTH legs, so the
    // array is never grown and the loop below silently skips reading the IBPs
    // the save wrote -> stream desync. The flag is on the STREAM (+0x08).
    // Same defect as WorkPlaceComponent (21b3b33); see
    // reference_serializer_plus8_is_encoding_not_loading_flag.
    const uintptr_t acStream = *reinterpret_cast<uintptr_t*>(ser);
    if (acStream && *reinterpret_cast<int32_t*>(acStream + 8) == 0 && s_fn_AC_ArrayGrow)
        s_fn_AC_ArrayGrow(reinterpret_cast<void*>(self + 0x28), count);

    int64_t* q = *reinterpret_cast<int64_t**>(self + 0x28);
    for (int32_t i = 0; i < count && q; ++i, ++q) {
        if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(q, ser);
        if (*q == 0) continue;
        // Value check, not just pointer: a registered-but-unfilled slot reads 0 and
        // IsDerivedFrom(x, 0) would drop every attendee.
        if (s_pTId_CitizenComponent && *s_pTId_CitizenComponent && fn_IsDerivedFrom &&
            !fn_IsDerivedFrom(reinterpret_cast<void*>(*q), *s_pTId_CitizenComponent))
            *q = 0;
    }

    s_fn_Serializer_Dtor(ser);
}

// ============================================================
// Vtable slots 19/20/21 — the description accessors
// ============================================================
// All three are one-instruction bodies in retail:
//   19 (0x1400026e0) MOV RAX,[0x1401c3f80]; RET   — the AttendanceDescription TypeInfo
//   20 (0x1400026d0) MOV EAX,[0x1401c3f78]; RET   — the AttendanceDescription type id
//   21 (0x140044700) MOV EAX,1;             RET
// 19/20 read the .rdata pair that ComponentEntity uses to match a component to the
// Description class it is built from. In standalone that .rdata is not mapped, so the
// faithful answers come from the live type registry / the registered type-id storage
// (0x1401c3f78 is filled by reg_economy_typeids.cpp) rather than from the globals.
//
// ⚠ Slot 21's retail body is at 0x140044700, NOT 0x140047000 — the coverage harness
// carried a transposed digit and pointed at CemeteryDescription_sub_6fc0.
#include "Object/factory_registration.h"

static void* AttendanceComponent_GetDescTypeInfo(void*) {
    static void* cached = nullptr;
    if (!cached) cached = ::Object::GetTypeInfoByName("AttendanceDescription");
    return cached;
}
static uint32_t AttendanceComponent_GetDescTypeId(void*) {
    static uint32_t* storage = nullptr;
    if (!storage) storage = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c3f78));
    return storage ? *storage : 0;
}
static int AttendanceComponent_Slot21Const(void*) { return 1; }

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

// Promoted 2026-08-15 from the hand-expanded inert factory (which reached only slots
// 13/14) to a full live ControllerComponent.
//
// The previous wave made the school's work checkbox clickable, but clicking it drove a
// state machine that did not exist: slots 16/17 (GetDefaultState / CreateState) still
// held the inert stub, so the StateMachineComponent never linked the embedded node and
// neither state 2 (open) nor state 3 (closed) ever ran. That is the "live command pair,
// inert controller" shape the coverage harness reported as 12/22.
//
// Size 0x58 / Flags 0x8a / NodeOff 0x38 / DefState 2 all read off
// AttendanceComponent_Ctor (0x140044580): Component_Ctor's 0x80 OR'd with the leaf's
// 0xa; the node vtable written to +0x38 and the owning-component back-pointer to +0x50
// (= node + 0x18); and slot 16's `MOV EAX,2; RET`.
//
// ⚠ NodeOff is 0x38, not the ResourceProduce family's 0x28. AttendanceComponent is not
// an RPC subclass and shares none of that table.
extern "C" void** AttendanceCtrl_NodeVtable();      // attendancecomponent_states.cpp
extern "C" void   AttendanceCtrl_PopulateTable();
extern "C" void*  ControllerLive_ScalarDeletingDtor(void*, unsigned int);

static void AttendanceCtrl_PopulateAndWireSlots();

CONTROLLER_LIVE_FACTORY_STATES(AttendanceComponent, 0x58, 0x8a, 0x38, 2,
                               "BANISHED_LIVE_ATTENDANCE",
                               AttendanceCtrl_NodeVtable, AttendanceCtrl_PopulateAndWireSlots)

static void AttendanceCtrl_PopulateAndWireSlots() {
    // From inside InstallLive only — FillTypedVtable re-stubs the shared vtable on every
    // instance creation, so every real slot must be (re)installed from here.
    AttendanceComponent_LiveVtable[3]  = reinterpret_cast<void*>(&AttendanceComponent_SerializeSlot);
    AttendanceComponent_LiveVtable[7]  = reinterpret_cast<void*>(&AttendanceComponent_Unregister);
    AttendanceComponent_LiveVtable[9]  = reinterpret_cast<void*>(&AttendanceComponent_Update);
    AttendanceComponent_LiveVtable[13] = reinterpret_cast<void*>(&AttendanceComponent_CommandMask);
    AttendanceComponent_LiveVtable[14] = reinterpret_cast<void*>(&AttendanceComponent_Command);
    AttendanceComponent_LiveVtable[18] = reinterpret_cast<void*>(&ControllerLive_ScalarDeletingDtor);
    AttendanceComponent_LiveVtable[19] = reinterpret_cast<void*>(&AttendanceComponent_GetDescTypeInfo);
    AttendanceComponent_LiveVtable[20] = reinterpret_cast<void*>(&AttendanceComponent_GetDescTypeId);
    AttendanceComponent_LiveVtable[21] = reinterpret_cast<void*>(&AttendanceComponent_Slot21Const);
    AttendanceCtrl_PopulateTable();
}
REGISTER_TYPE(AttendanceComponent, "ControllerComponent",
              &AttendanceComponent_Factory, &AttendanceComponent_CreateInPlace);
