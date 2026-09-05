// code/Game/Work/pickup.cpp
// Clean C++ reimplementation of Pickup -- all methods.
// Source: Game/Work/Pickup.cpp (from profile build debug strings)
//
// Pickup (0x60 bytes) -- Item pickup work action.
// Inherits: Object::Internal -> Work (0x48) -> Pickup
// Vtable: 0x140161810
//
// Pickup differs from most Work classes:
//   - Has its OWN Execute override (re-targets when citizen changes workplace)
//   - Fields: status (+0x48), citizenIBP (+0x50), candidateList (+0x58)
//   - Status values: 0=unassigned, 1=assigned, 2=completing
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/pickup_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Object/factory_registration.h"  // Object::GetTypeIdByName (.rdata tid fallback)

// ============================================================
// BANISHED_RAWMAT_PICKUP_TRACE -- shared diagnostic gate.
// ============================================================
static bool PickupTrace() {
    static const bool t = [] {
        const char* e = std::getenv("BANISHED_RAWMAT_PICKUP_TRACE");
        return e && e[0] && e[0] != '0';
    }();
    return t;
}

// ============================================================
// Local function pointers (resolved once by Pickup_ResolveFuncs)
// ============================================================

// All typedefs prefixed with PiW_ to avoid conflicts

// Work::Serialize
typedef void  (*PiW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor/Serialize/GetVersion
typedef void* (*PiW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*PiW_Serializer_Dtor_t)(void* self);
typedef void  (*PiW_Serializer_Serialize_t)(void* self, int* value, const char* name);
typedef int   (*PiW_Serializer_GetVersion_t)(void* self);
// Object::InternalBasePtr::Serialize
typedef void  (*PiW_InternalBasePtr_Serialize_t)(void* basePtr, void* serializer);
// FUN_140018020 -- array/list serialization helper
typedef void  (*PiW_ArraySerialize_t)(void* arr, void* serializer);
// FUN_1400f7100 -- validate work entity
typedef bool  (*PiW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// FUN_1400c5400 -- cancel all work items
typedef void  (*PiW_CancelAllWork_t)(void* actionStack);
// Object::Internal::~Internal (dtor tail call)
typedef void  (*PiW_Internal_Dtor_t)(void* self);
// fn_MemFreeStatic
typedef void  (*PiW_MemFreeStatic_t)(void* ptr);
// Slot4/CreateWork -- original function (called through for complex logic)
typedef int   (*PiW_Slot4_Original_t)(void* self, void* entityIBP, uint64_t param3, uint64_t param4);

static PiW_Work_Serialize_t            s_fn_Work_Serialize = nullptr;
static PiW_Serializer_Ctor_t           s_fn_Serializer_Ctor = nullptr;
static PiW_Serializer_Dtor_t           s_fn_Serializer_Dtor = nullptr;
static PiW_Serializer_Serialize_t      s_fn_Serializer_Serialize = nullptr;
static PiW_Serializer_GetVersion_t     s_fn_Serializer_GetVersion = nullptr;
static PiW_InternalBasePtr_Serialize_t s_fn_InternalBasePtr_Serialize = nullptr;
static PiW_ArraySerialize_t            s_fn_ArraySerialize = nullptr;
static PiW_ValidateEntity_t            s_fn_ValidateEntity = nullptr;
static PiW_CancelAllWork_t             s_fn_CancelAllWork = nullptr;
static PiW_MemFreeStatic_t             s_fn_MemFreeStatic = nullptr;
static PiW_Slot4_Original_t            s_fn_Slot4_Original = nullptr;

// Type ID for validation (Pickup type at DAT_1401c7158)
static uint32_t s_typeId_Pickup = 0;

// Type index constant at DAT_1401c6be8 (for Slot1)
static uint64_t* s_typeIndex_Pickup = nullptr;

// Vtable pointers for dtor chain
static void** s_vtable_Pickup = nullptr;  // 0x140161810
static void** s_vtable_Work = nullptr;    // 0x140161a80

static bool s_pickup_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool Pickup_ResolveFuncs() {
    if (s_pickup_funcs_resolved) return true;
    // PERF (2026-09-05): one dependency stays unresolved in standalone, so `ok` below is
    // false and this table was re-resolved on EVERY Pickup tick (1% of the bucket phase).
    // Resolve once; the result (false) is what every caller already saw.
    static bool s_attempted = false;
    if (s_attempted) return false;
    s_attempted = true;

    // Direct exe function addresses
    s_fn_Work_Serialize = reinterpret_cast<PiW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_ArraySerialize = reinterpret_cast<PiW_ArraySerialize_t>(
        ResolveAddr(Module::Exe, 0x140018020));
    s_fn_ValidateEntity = reinterpret_cast<PiW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_CancelAllWork = reinterpret_cast<PiW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_Slot4_Original = reinterpret_cast<PiW_Slot4_Original_t>(
        ResolveAddr(Module::Exe, 0x1400f2f00));

    // Dispatch slots (indirect through .rdata)
    // Serializer ctor/dtor -- use same slots as Pickup's Serialize (0x140152ad8/0x140152ad0)
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<PiW_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<PiW_Serializer_Dtor_t*>(iatSerDtor);

    // Serializer::Serialize
    uintptr_t iatSer = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSer)
        s_fn_Serializer_Serialize = *reinterpret_cast<PiW_Serializer_Serialize_t*>(iatSer);

    // Serializer::GetVersion (correct IAT slot verified from original disasm)
    uintptr_t iatGetVer = ResolveAddr(Module::Exe, 0x140152a48);
    if (iatGetVer)
        s_fn_Serializer_GetVersion = *reinterpret_cast<PiW_Serializer_GetVersion_t*>(iatGetVer);

    // InternalBasePtr::Serialize
    uintptr_t iatBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatBPSer)
        s_fn_InternalBasePtr_Serialize = *reinterpret_cast<PiW_InternalBasePtr_Serialize_t*>(iatBPSer);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<PiW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Type ID -- read uint32 value at 0x1401c7158
    uintptr_t typeIdAddr = ResolveAddr(Module::Exe, 0x1401c7158);
    if (typeIdAddr)
        s_typeId_Pickup = *reinterpret_cast<uint32_t*>(typeIdAddr);

    // Type index constant at DAT_1401c6be8 (for Slot1)
    s_typeIndex_Pickup = reinterpret_cast<uint64_t*>(
        ResolveAddr(Module::Exe, 0x1401c6be8));

    // Vtable pointers for dtor
    s_vtable_Pickup = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161810));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161a80));

    // Verify critical pointers
    bool ok = s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serializer_Serialize && s_fn_Serializer_GetVersion &&
              s_fn_InternalBasePtr_Serialize && s_fn_ArraySerialize &&
              s_fn_ValidateEntity && s_fn_CancelAllWork &&
              s_fn_Slot4_Original &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && g_pGameWorldSingleton &&
              s_vtable_Pickup && s_vtable_Work;

    s_pickup_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[Pickup] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_Serializer_Serialize) printf("  MISSING: Serializer_Serialize\n");
        if (!s_fn_Serializer_GetVersion) printf("  MISSING: Serializer_GetVersion\n");
        if (!s_fn_InternalBasePtr_Serialize) printf("  MISSING: InternalBasePtr_Serialize\n");
        if (!s_fn_ArraySerialize) printf("  MISSING: ArraySerialize\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_Slot4_Original) printf("  MISSING: Slot4_Original\n");
        if (!fn_InternalBasePtr_CheckPtr) printf("  MISSING: InternalBasePtr_CheckPtr\n");
        if (!fn_InternalBasePtr_Ctor) printf("  MISSING: InternalBasePtr_Ctor\n");
        if (!fn_IsDerivedFrom) printf("  MISSING: IsDerivedFrom\n");
        if (!g_pGameWorldSingleton) printf("  MISSING: g_pGameWorldSingleton\n");
    }
#endif

    return ok;
}


// ============================================================
// Pickup::Serialize (0x1400f3aa0)
// Pattern: Work_Serialize + Serializer(v2)
//   + Serializer::Serialize(status at +0x48)
//   + InternalBasePtr::Serialize(citizenIBP at +0x50) + IsDerivedFrom validate
//   + if version > 0: ArraySerialize(candidateList at +0x58)
// ============================================================
void Pickup_Serialize_Impl(Pickup* self, void* stream) {
    Pickup_ResolveFuncs();   // lazy-resolve — factory-bypassed Pickups (see Slot9 note)
    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create serializer (version=2)
    uint8_t serializerBuf[24] = {0};
    s_fn_Serializer_Ctor(serializerBuf, stream, 2);

    // Serialize uint32 status at +0x48
    s_fn_Serializer_Serialize(serializerBuf, reinterpret_cast<int*>(&self->status), nullptr);

    // Serialize InternalBasePtr at +0x50
    s_fn_InternalBasePtr_Serialize(&self->citizenIBP, serializerBuf);

    // Validate the deserialized ptr against Pickup type
    if (self->citizenIBP != nullptr) {
        if (!fn_IsDerivedFrom(self->citizenIBP, s_typeId_Pickup)) {
            self->citizenIBP = nullptr;
        }
    }

    // Version >= 1: serialize candidate list at +0x58
    int version = s_fn_Serializer_GetVersion(serializerBuf);
    if (version > 0) {
        s_fn_ArraySerialize(&self->candidateList, serializerBuf);
    }

    s_fn_Serializer_Dtor(serializerBuf);
}


// ============================================================
// Pickup::Execute (vtable +0x20, 0x1400f2f00)
//
// NAMING: Ghidra labels 0x1400f2f00 "Pickup_Serialize". It is NOT Serialize
// (that is 0x1400f3aa0) -- it is the real Execute, vtable slot 4. Verified by
// reading the vtable bytes at 0x140161810: [4]=0x1400f2f00, [5]=0x1400f38f0.
//
// WHAT IT DOES (plain terms): when this Pickup work is still unclaimed
// (status==0), a laborer runs it to claim the pile and start hauling:
//   1. Look up the target pile's RawMaterialComponent (0x1400223c0) and its
//      RawMaterialDescription (IsDerivedFrom vs DAT_1401c7cb8).
//   2. Compute the citizen's free carry capacity: StorageComponent+0x38 minus
//      StorageComponent::QueryCount (0x14012cf10). If one pile's worth
//      (desc+0x48 unitSize * rawmat+0x28 amount) does not fit -> tell the
//      citizen to re-plan (CitizenAI_State_LongTermPlan) and return 2.
//   3. Derive the citizen's cell from its ModelComponent Transform position
//      (model+0x158/+0x15c, i.e. Transform@+0x148 pos@+0x10/+0x14), clamped.
//   4. Seed a candidate array with this Pickup's own target, then sweep the
//      11x11 cell neighbourhood (dy,dx in -5..+5, skipping the centre) around
//      the *pile*, collecting every other reachable pile of the same kind whose
//      per-type Pickup work is not already claimed (vtable slot 11 == false).
//   5. Sort candidates nearest-first (0x14002c070 = std::sort by +0x08 distance;
//      reimplemented locally as Pickup_SortByDistance -- see the note there).
//   6. For each candidate that still fits in the remaining capacity: convert its
//      access cell to a world position and dispatch PerformWork (0x1400c0ad0) --
//      THIS is what actually makes the citizen walk over and haul the pile --
//      then mark that per-type Pickup work claimed (+0x48=1, +0x50=citizen).
//   7. Stamp the citizen (+0x30 = work type id, +0x54 |= 4, +0xb0 = 0, and
//      +0x88 |= 0x20000 when the work targets the citizen's own workplace).
//
// Returns 0/1/2 as an int (never bool -- trampoline bug class #7): 1 = no work
// available (also the early-out when status != 0), 2 = work dispatched OR the
// citizen is full and must re-plan, 0 = nothing dispatched this tick.
//
// The candidate record is 32 bytes; layout recovered from the stores at
// 0x1400f320c-0x1400f3221 and the reads at 0x1400f3712-0x1400f378a:
//   +0x00 int   cell        (pile cell index)
//   +0x04 int   accessCell  (walkable cell the citizen stands on; 0x14009ccf0)
//   +0x08 int   distance    (0x140008830) <- the sort key used by 0x14002c070
//   +0x0c int   typeId      (resource type id; BST key into gw+0x118)
//   +0x10 IBP   entity      (the pile entity)
//   +0x18 IBP   rawMat      (its RawMaterialComponent)
// ============================================================

struct PickupCandidate {
    int32_t cell;        // +0x00
    int32_t accessCell;  // +0x04
    int32_t distance;    // +0x08
    int32_t typeId;      // +0x0c
    void*   entity;      // +0x10
    void*   rawMat;      // +0x18
};
static_assert(sizeof(PickupCandidate) == 0x20, "PickupCandidate must be 32 bytes");

// 0x14002c070 is MSVC's inlined std::sort over these 32-byte records, ordering
// ascending on the int32 at +0x08 (distance): median-of-3 introsort with an
// explicit range stack, falling back to a selection sort (0x14002c380) once a
// range is <= 8 elements. Both are unstable, so std::sort reproduces the
// ordering semantics exactly; only the tie-break among equal-distance
// candidates may differ, which no caller observes. Reimplemented rather than
// registered -- porting a hand-rolled introsort would add nothing.
static void Pickup_SortByDistance(PickupCandidate* buf, int count) {
    std::sort(buf, buf + count,
              [](const PickupCandidate& a, const PickupCandidate& b) {
                  return a.distance < b.distance;
              });
}

// ---- sub-callees used only by the Execute port (PiX_ prefix) ----
typedef void     (*PiX_FindRawMat_t)(int64_t entity, void* outIBP);                  // 0x1400223c0
typedef void*    (*PiX_FindStorage_t)(int64_t inner, void* outIBP);                  // 0x140006270
typedef uint32_t (*PiX_QueryCount_t)(void* storage);                                 // 0x14012cf10
typedef uint64_t (*PiX_LongTermPlan_t)(int64_t citizen);                             // 0x140115e60
typedef char     (*PiX_WantsResource_t)(void* townMgr, int64_t desc, int amount);    // 0x14008d3c0
typedef int      (*PiX_FindCell_t)(int* grid, int from, int to, uint32_t mask);      // 0x14009ccf0
typedef uint64_t (*PiX_ValidateCell_t)(int* grid, int mask, uint64_t a, uint32_t b); // 0x1400a00f0
typedef char     (*PiX_CheckAssign_t)(void* self, void* ibp, uint32_t a,
                                      uint32_t b, void* time);                       // 0x1400f6ef0
typedef void*    (*PiX_FindStoreForRes_t)(void* townMgr, void* out, uint32_t access,
                                          uint64_t resType, int amount, char useCanAdd,
                                          float* outPos, void* citizenIBP);          // 0x14008d590
typedef int      (*PiX_CalcDistance_t)(int* grid, uint32_t a, uint32_t b);           // 0x140008830
typedef char     (*PiX_CanAdd_t)(void* storage, int64_t entity, int qty);            // 0x14012cd80
typedef void*    (*PiX_FindComp_t)(int64_t entity, void* outIBP);                    // 0x140003050
typedef void     (*PiX_GridToWorld_t)(int* grid, float* outPos, const float* in,
                                      char flag, void* p5);                          // 0x14009f3e0
typedef void     (*PiX_PerformWork_t)(void* citizenIBP, uint32_t typeId, void* entityIBP,
                                      float* pos, int cell, char first);             // 0x1400c0ad0
typedef void*    (*PiX_MemAlloc_t)(uint64_t size, uint32_t align);                   // slot 0x140152b60
typedef void     (*PiX_MemFree_t)(void* p);                                          // slot 0x140152b68
typedef void     (*PiX_V3Ctor_t)(void* self, float x, float y, float z);             // slot 0x140152a30

static PiX_FindRawMat_t      s_x_findRawMat   = nullptr;
static PiX_FindStorage_t     s_x_findStorage  = nullptr;
static PiX_QueryCount_t      s_x_queryCount   = nullptr;
static PiX_LongTermPlan_t    s_x_longTermPlan = nullptr;
static PiX_WantsResource_t   s_x_wantsRes     = nullptr;
static PiX_FindCell_t        s_x_findCell     = nullptr;
static PiX_ValidateCell_t    s_x_validateCell = nullptr;
static PiX_CheckAssign_t     s_x_checkAssign  = nullptr;
static PiX_FindStoreForRes_t s_x_findStoreRes = nullptr;
static PiX_CalcDistance_t    s_x_calcDist     = nullptr;
static PiX_CanAdd_t          s_x_canAdd       = nullptr;
static PiX_FindComp_t        s_x_findComp     = nullptr;
static PiX_GridToWorld_t     s_x_gridToWorld  = nullptr;
static PiX_PerformWork_t     s_x_performWork  = nullptr;
static PiX_MemAlloc_t        s_x_memAlloc     = nullptr;
static PiX_MemFree_t         s_x_memFree      = nullptr;
static PiX_V3Ctor_t          s_x_v3ctor       = nullptr;

// Type-id globals. Raw .rdata resolves to NULL in standalone (see
// reference_standalone_rdata_null_fallback), so each falls back to the runtime
// name lookup. Verified in the disassembly of 0x1400f2f00:
//   0x1401c7cb8 -> RawMaterialDescription (MOV EDX,[0x1401c7cb8] @0x1400f2f75)
//   0x1401c6be0 -> Pickup (MOV EDX,[0x1401c6be0] @0x1400f34cd/@0x1400f37f3).
//     This is TypeInfo_Pickup, i.e. what Pickup::GetTypeId (0x14002bfa0) returns.
//     NOTE: it is NOT the file-level s_typeId_Pickup, which reads 0x1401c7158 --
//     that global is the *CitizenComponent* type id, used by Pickup::Serialize to
//     validate the citizen at +0x50. The name there is misleading; do not reuse it.
static const uint32_t* s_x_pTid_RawMatDesc = nullptr;  // 0x1401c7cb8
static const uint32_t* s_x_pTid_Pickup     = nullptr;  // 0x1401c6be0

static bool s_x_resolved = false;

static void Pickup_ResolveExecFuncs() {
    if (s_x_resolved) return;
    s_x_resolved = true;

    auto code = [](uintptr_t a) -> void* {
        return reinterpret_cast<void*>(ResolveAddr(Module::Exe, a));
    };
    // Dispatch slots hold the Runtime fn-ptr; deref the slot.
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };

    s_x_findRawMat   = reinterpret_cast<PiX_FindRawMat_t>(code(0x1400223c0));
    s_x_findStorage  = reinterpret_cast<PiX_FindStorage_t>(code(0x140006270));
    s_x_queryCount   = reinterpret_cast<PiX_QueryCount_t>(code(0x14012cf10));
    s_x_longTermPlan = reinterpret_cast<PiX_LongTermPlan_t>(code(0x140115e60));
    s_x_wantsRes     = reinterpret_cast<PiX_WantsResource_t>(code(0x14008d3c0));
    s_x_findCell     = reinterpret_cast<PiX_FindCell_t>(code(0x14009ccf0));
    s_x_validateCell = reinterpret_cast<PiX_ValidateCell_t>(code(0x1400a00f0));
    s_x_checkAssign  = reinterpret_cast<PiX_CheckAssign_t>(code(0x1400f6ef0));
    s_x_findStoreRes = reinterpret_cast<PiX_FindStoreForRes_t>(code(0x14008d590));
    s_x_calcDist     = reinterpret_cast<PiX_CalcDistance_t>(code(0x140008830));
    s_x_canAdd       = reinterpret_cast<PiX_CanAdd_t>(code(0x14012cd80));
    s_x_findComp     = reinterpret_cast<PiX_FindComp_t>(code(0x140003050));
    s_x_gridToWorld  = reinterpret_cast<PiX_GridToWorld_t>(code(0x14009f3e0));
    s_x_performWork  = reinterpret_cast<PiX_PerformWork_t>(code(0x1400c0ad0));

    s_x_memAlloc = reinterpret_cast<PiX_MemAlloc_t>(slot(0x140152b60));
    if (!s_x_memAlloc) s_x_memAlloc = reinterpret_cast<PiX_MemAlloc_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAXII@Z"));
    s_x_memFree = reinterpret_cast<PiX_MemFree_t>(slot(0x140152b68));
    if (!s_x_memFree) s_x_memFree = reinterpret_cast<PiX_MemFree_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    s_x_v3ctor = reinterpret_cast<PiX_V3Ctor_t>(slot(0x140152a30));
    if (!s_x_v3ctor) s_x_v3ctor = reinterpret_cast<PiX_V3Ctor_t>(
        ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));

    s_x_pTid_RawMatDesc = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_x_pTid_Pickup = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6be0));

    if (PickupTrace()) {
        fprintf(stderr,
            "[RawMatPickup] Execute deps: wantsRes(8d3c0)=%p "
            "performWork(c0ad0)=%p findStoreRes(8d590)=%p checkAssign(f6ef0)=%p "
            "ltp(115e60)=%p memAlloc=%p v3ctor=%p\n",
            (void*)s_x_wantsRes, (void*)s_x_performWork,
            (void*)s_x_findStoreRes, (void*)s_x_checkAssign,
            (void*)s_x_longTermPlan, (void*)s_x_memAlloc, (void*)s_x_v3ctor);
        fflush(stderr);
    }
}

// DAT_140172114 = 0x3f000000 = 0.5f. Raw .rdata -> NULL in standalone; real-value
// fallback (bit pattern confirmed via Ghidra read_memory, and independently
// documented in code/Game/Action/action_pf_geom.cpp).
static float Pickup_CellCenterHalf() {
    static const float* p = reinterpret_cast<const float*>(
        ResolveAddr(Module::Exe, 0x140172114));
    return p ? *p : 0.5f;
}

// Type-id read with runtime-name fallback (raw .rdata is NULL in standalone).
static uint32_t Pickup_Tid(const uint32_t* p, const char* name) {
    if (p && *p) return *p;
    return ::Object::GetTypeIdByName(name);
}

// Inlined Array<PickupCandidate>::Add. Mirrors the exe growth idiom byte-for-byte
// (0x1400f3545-0x1400f3630): capacity becomes newCount, or newCount*3 when the
// array was already non-empty; on realloc the old elements are copied and the old
// buffer freed; without realloc only the new tail element is written.
static void Pickup_CandAdd(PickupCandidate*& buf, int& count, int& capacity,
                           const PickupCandidate& v) {
    const int oldCount = count;
    const int newCount = oldCount + 1;
    PickupCandidate* oldBuf = buf;
    count = newCount;

    if (newCount > capacity) {
        capacity = newCount;
        if (oldCount != 0) capacity = newCount * 3;
        buf = s_x_memAlloc
            ? reinterpret_cast<PickupCandidate*>(
                  s_x_memAlloc(static_cast<uint64_t>(capacity) * 0x20, 8))
            : nullptr;
    }
    if (!buf) { buf = oldBuf; count = oldCount; return; }  // alloc failed -> no-op

    for (int i = (oldBuf == buf) ? oldCount : 0; i < newCount; ++i) {
        buf[i] = (i < oldCount && oldBuf) ? oldBuf[i] : v;
    }
    if (oldBuf != buf && oldBuf && s_x_memFree) s_x_memFree(oldBuf);
}

// The port proper. Kept separate from Pickup_Slot4_Impl so the dispatcher can
// fall back to the original when it resolves.
static int Pickup_Execute_Port(Pickup* self, void* citizenIBP, void* time) {
    // 0x1400f2f37: CMP [RCX+0x48],0 -> anything but "unassigned" means no work.
    if (self->status != 0) return 1;

    Pickup_ResolveExecFuncs();

    // The whole body is inert without these three, and half-executing would leave
    // a Pickup marked claimed with no action behind it (silent corruption, strictly
    // worse than doing nothing -- cf. code/Game/Work/work_leaf_gather.cpp). Bail as
    // "no work available" instead.
    if (!s_x_wantsRes || !s_x_performWork || !s_x_findRawMat || !s_x_findStorage ||
        !s_x_queryCount || !s_x_findCell || !s_x_validateCell || !s_x_checkAssign ||
        !s_x_findStoreRes || !s_x_calcDist || !s_x_canAdd || !s_x_findComp ||
        !s_x_gridToWorld || !s_x_memAlloc || !s_x_memFree || !s_x_v3ctor ||
        !fn_InternalBasePtr_CheckPtr || !fn_InternalBasePtr_Ctor ||
        !fn_IsDerivedFrom || !g_pGameWorldSingleton) {
        if (PickupTrace()) {
            static int warned = 0;
            if (++warned <= 3)
                fprintf(stderr, "[RawMatPickup] Execute INERT: unresolved dep(s)\n");
            fflush(stderr);
        }
        return 1;
    }

    const uint32_t tidRawMatDesc = Pickup_Tid(s_x_pTid_RawMatDesc, "RawMaterialDescription");
    const uint32_t tidPickup     = Pickup_Tid(s_x_pTid_Pickup, "Pickup");

    const uintptr_t S = reinterpret_cast<uintptr_t>(self);
    void* targetIBP = reinterpret_cast<void*>(S + 0x10);  // RSI: Work's target entity IBP

    // --- pile's RawMaterialComponent + its RawMaterialDescription ---------------
    fn_InternalBasePtr_CheckPtr(targetIBP);
    void* rawMatIBP = nullptr;
    s_x_findRawMat(*reinterpret_cast<int64_t*>(targetIBP), &rawMatIBP);
    fn_InternalBasePtr_CheckPtr(&rawMatIBP);
    void* descRaw = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(rawMatIBP) + 0x18);
    void* desc = fn_IsDerivedFrom(descRaw, tidRawMatDesc) ? descRaw : nullptr;

    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    void* townMgr = *reinterpret_cast<void**>(gw + 0xf0);
    int*  grid    = *reinterpret_cast<int**>(gw + 0xf8);

    // --- citizen's free carry capacity -----------------------------------------
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citizen = *reinterpret_cast<uintptr_t*>(citizenIBP);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(citizen + 0x10));
    void* storageIBP = nullptr;
    s_x_findStorage(*reinterpret_cast<int64_t*>(citizen + 0x10), &storageIBP);
    fn_InternalBasePtr_CheckPtr(&storageIBP);
    int cap = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(storageIBP) + 0x38);
    fn_InternalBasePtr_CheckPtr(&storageIBP);
    int freeCap = cap - static_cast<int>(s_x_queryCount(storageIBP));

    fn_InternalBasePtr_CheckPtr(&rawMatIBP);
    // desc+0x48 = per-unit size, rawMat+0x28 = amount in the pile. (The original
    // dereferences desc unguarded here; kept faithful -- desc is non-null whenever
    // 0x1400223c0 found a RawMaterialComponent, which is the only way to get here.)
    const int needed = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x48) *
                       *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatIBP) + 0x28);
    if (freeCap < needed) {
        fn_InternalBasePtr_CheckPtr(citizenIBP);
        s_x_longTermPlan(*reinterpret_cast<int64_t*>(citizenIBP));
        return 2;  // citizen is full -> re-plan
    }

    // --- citizen's cell, from its ModelComponent transform ----------------------
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t c = *reinterpret_cast<uintptr_t*>(citizenIBP);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(c + 0x278));
    uintptr_t sm = *reinterpret_cast<uintptr_t*>(c + 0x278);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(sm + 0x68));
    uintptr_t model = *reinterpret_cast<uintptr_t*>(sm + 0x68);

    // Scratch Vector3 shared by 0x14008d590's out-position and the later
    // Vector3 ctor (both write all three floats). The exe pre-seeds only .z from
    // model+0x160 at 0x1400f3070 and never reads it back -- 0x14008d590 writes
    // param_7[0..2] unconditionally at its return. Kept for fidelity; it is dead.
    float scratch[3] = {0.0f, 0.0f, 0.0f};
    scratch[2] = *reinterpret_cast<float*>(model + 0x160);

    const int dim = *grid;
    int cx = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) +
                             static_cast<float>(dim >> 1));
    int cz = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) +
                             static_cast<float>(dim >> 1));
    if (cx < 0)    cx = 0;
    if (cx >= dim) cx = dim - 1;
    if (cz < 0)    cz = 0;
    if (cz >= dim) cz = dim - 1;
    const int citizenCell = cz * dim + cx;

    // Work+0x28 = this work's target cell (32-bit; the decompiler widens it to 64).
    const int selfCell = *reinterpret_cast<int*>(S + 0x28);
    const int pileCol  = selfCell % dim;
    const int pileRow  = selfCell / dim;

    // --- gate: does the town still want this resource? --------------------------
    fn_InternalBasePtr_CheckPtr(&rawMatIBP);
    int amount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatIBP) + 0x28);
    fn_InternalBasePtr_CheckPtr(targetIBP);
    if (!s_x_wantsRes(townMgr,
                      *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x28),
                      amount))
        return 1;

    const int selfAccess = s_x_findCell(grid, selfCell, citizenCell, 0xe07);
    if (selfAccess < 0) return 1;
    if (!s_x_validateCell(grid, 0xe1f, static_cast<uint64_t>(static_cast<uint32_t>(selfAccess)),
                          static_cast<uint32_t>(citizenCell)))
        return 1;

    void* assignIBP = *reinterpret_cast<void**>(citizenIBP);
    if (!s_x_checkAssign(self, &assignIBP, static_cast<uint32_t>(selfAccess),
                         static_cast<uint32_t>(citizenCell), time))
        return 0;

    // --- is there anywhere to drop it off? --------------------------------------
    void* dropIBP = nullptr;
    void* storeOut = nullptr;
    fn_InternalBasePtr_CheckPtr(&rawMatIBP);
    amount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatIBP) + 0x28);
    fn_InternalBasePtr_CheckPtr(targetIBP);
    s_x_findStoreRes(townMgr, &storeOut, static_cast<uint32_t>(selfAccess),
                     *reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x28),
                     amount, 1, scratch, &dropIBP);
    if (storeOut == nullptr) return 1;

    // Storage we will haul into -- used to test each candidate for acceptance.
    fn_InternalBasePtr_CheckPtr(&storeOut);
    void* dstStorageIBP = nullptr;
    s_x_findStorage(reinterpret_cast<int64_t>(storeOut), &dstStorageIBP);

    // --- seed the candidate array with this Pickup's own target -----------------
    PickupCandidate cand;
    cand.cell       = selfCell;
    cand.accessCell = selfAccess;
    cand.distance   = s_x_calcDist(grid, static_cast<uint32_t>(citizenCell),
                                   static_cast<uint32_t>(selfCell));
    cand.typeId     = *reinterpret_cast<int32_t*>(S + 0x20);
    cand.entity     = *reinterpret_cast<void**>(targetIBP);
    cand.rawMat     = rawMatIBP;

    int count = 1;
    int capacity = 1;
    PickupCandidate* buf =
        reinterpret_cast<PickupCandidate*>(s_x_memAlloc(0x20, 8));
    if (!buf) return 1;
    buf[0] = cand;
    // 0x1400f32cd: the exe frees the *previous* (null) buffer here -- a literal
    // MemFree(0) no-op. Mirrored for fidelity.
    s_x_memFree(nullptr);

    // --- sweep the 11x11 neighbourhood around the pile --------------------------
    const uintptr_t bstRoot = gw + 0x118;  // work/task registry; BST at +0x30

    int row = pileRow - 5;
    for (int dy = -5; dy <= 5; ++dy, ++row) {
        for (int dx = -5; dx <= 5; ++dx) {
            if (dx == 0 && dy == 0) continue;  // skip the pile's own cell
            const int col = pileCol + dx;
            if (col < 0 || col >= dim) continue;
            if (row < 0 || row >= dim) continue;

            const int nCell = row * dim + col;
            uintptr_t cellPtr =
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80) +
                static_cast<uintptr_t>(static_cast<int64_t>(nCell)) * 0x30;

            // Occupant: prefer cell+0x00, else cell+0x08, else a null IBP.
            void* occIBP = nullptr;
            if (*reinterpret_cast<uintptr_t*>(cellPtr) != 0) {
                occIBP = *reinterpret_cast<void**>(cellPtr);
            } else if (*reinterpret_cast<uintptr_t*>(cellPtr + 8) != 0) {
                occIBP = *reinterpret_cast<void**>(cellPtr + 8);
            } else {
                fn_InternalBasePtr_Ctor(&occIBP, nullptr);
            }
            if (occIBP == nullptr) continue;

            fn_InternalBasePtr_CheckPtr(&occIBP);
            void* nRawMatIBP = nullptr;
            s_x_findRawMat(reinterpret_cast<int64_t>(occIBP), &nRawMatIBP);
            if (nRawMatIBP == nullptr) continue;

            fn_InternalBasePtr_CheckPtr(&occIBP);
            int64_t nDesc = *reinterpret_cast<int64_t*>(
                reinterpret_cast<uintptr_t>(occIBP) + 0x28);
            fn_InternalBasePtr_CheckPtr(&dstStorageIBP);
            if (!s_x_canAdd(dstStorageIBP, nDesc, 1)) continue;

            const int nAccess = s_x_findCell(grid, nCell, citizenCell, 0xe07);
            if (nAccess < 0) continue;
            if (!s_x_validateCell(grid, 0xe1f,
                                  static_cast<uint64_t>(static_cast<uint32_t>(nAccess)),
                                  static_cast<uint32_t>(citizenCell)))
                continue;

            fn_InternalBasePtr_CheckPtr(&occIBP);
            void* compOut = nullptr;
            void* compIBP = s_x_findComp(reinterpret_cast<int64_t>(occIBP), &compOut);
            fn_InternalBasePtr_CheckPtr(compIBP);
            const int nTypeId = *reinterpret_cast<int*>(
                *reinterpret_cast<uintptr_t*>(compIBP) + 0x28);

            // Walk the type-id-keyed BST at gw+0x118 -> +0x30 for this resource's
            // Pickup work. Node: key @+0x00, value @+0x08, left @+0x18, right @+0x20.
            void* workIBP = nullptr;
            {
                int* node = *reinterpret_cast<int**>(*reinterpret_cast<uintptr_t*>(bstRoot) + 0x30);
                bool found = false;
                while (node != nullptr) {
                    if (nTypeId < *node) {
                        node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);
                    } else if (nTypeId > *node) {
                        node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x20);
                    } else {
                        workIBP = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x08);
                        found = true;
                        break;
                    }
                }
                if (!found) fn_InternalBasePtr_Ctor(&workIBP, nullptr);
            }

            fn_InternalBasePtr_CheckPtr(&workIBP);
            void* pickupRaw = workIBP;
            void* pickupWork = fn_IsDerivedFrom(pickupRaw, tidPickup) ? pickupRaw : nullptr;
            void* pickupIBP = nullptr;
            fn_InternalBasePtr_Ctor(&pickupIBP, pickupWork);
            if (pickupIBP == nullptr) continue;

            // vtable slot 11 (+0x58) = "already has a worker" -> skip this pile.
            fn_InternalBasePtr_CheckPtr(&pickupIBP);
            {
                typedef char (*HasWorker_t)(void*);
                auto hasWorker = *reinterpret_cast<HasWorker_t*>(
                    *reinterpret_cast<uintptr_t*>(pickupIBP) + 0x58);
                if (hasWorker && hasWorker(pickupIBP)) continue;
            }

            PickupCandidate n;
            n.cell       = nCell;
            n.accessCell = nAccess;
            n.distance   = s_x_calcDist(grid, static_cast<uint32_t>(citizenCell),
                                        static_cast<uint32_t>(nCell));
            n.typeId     = nTypeId;
            n.entity     = occIBP;
            n.rawMat     = nRawMatIBP;
            Pickup_CandAdd(buf, count, capacity, n);
        }
    }

    // --- nearest-first ----------------------------------------------------------
    if (count > 1) Pickup_SortByDistance(buf, count);

    // --- dispatch ---------------------------------------------------------------
    const float half = Pickup_CellCenterHalf();
    char first = 1;
    int  result = 0;

    for (int i = 0; i < count; ++i) {
        PickupCandidate* p = &buf[i];
        void* rmIBP = &p->rawMat;

        fn_InternalBasePtr_CheckPtr(rmIBP);
        void* dRaw = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(p->rawMat) + 0x18);
        void* d = fn_IsDerivedFrom(dRaw, tidRawMatDesc) ? dRaw : nullptr;

        fn_InternalBasePtr_CheckPtr(rmIBP);
        const int need = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(d) + 0x48) *
                         *reinterpret_cast<int*>(
                             reinterpret_cast<uintptr_t>(p->rawMat) + 0x28);
        if (need > freeCap) continue;
        freeCap -= need;

        // Access cell -> world position. off = (dim>>1) - 0.5
        const int gdim = *grid;
        const float off = static_cast<float>(gdim >> 1) - half;
        s_x_v3ctor(scratch,
                   static_cast<float>(p->accessCell % gdim) - off,
                   static_cast<float>(p->accessCell / gdim) - off,
                   0.0f);

        float worldPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        s_x_gridToWorld(grid, worldPos, scratch, 0, nullptr);

        void* entIBP = p->entity;
        void* citIBP = *reinterpret_cast<void**>(citizenIBP);
        s_x_performWork(&citIBP, static_cast<uint32_t>(p->typeId), &entIBP,
                        worldPos, p->cell, first);
        first = 0;

        if (PickupTrace()) {
            fprintf(stderr, "[RawMatPickup] PerformWork dispatched cand=%d/%d "
                            "cell=%d access=%d dist=%d typeId=%d\n",
                    i, count, p->cell, p->accessCell, p->distance, p->typeId);
            fflush(stderr);
        }

        // Mark this resource type's Pickup work as claimed by our citizen.
        void* wIBP = nullptr;
        {
            int* node = *reinterpret_cast<int**>(*reinterpret_cast<uintptr_t*>(bstRoot) + 0x30);
            bool found = false;
            while (node != nullptr) {
                if (p->typeId < *node) {
                    node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);
                } else if (p->typeId > *node) {
                    node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x20);
                } else {
                    wIBP = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x08);
                    found = true;
                    break;
                }
            }
            if (!found) fn_InternalBasePtr_Ctor(&wIBP, nullptr);
        }

        fn_InternalBasePtr_CheckPtr(&wIBP);
        void* wRaw = wIBP;
        void* wPickup = fn_IsDerivedFrom(wRaw, tidPickup) ? wRaw : nullptr;
        void* claimed = nullptr;
        fn_InternalBasePtr_Ctor(&claimed, wPickup);
        fn_InternalBasePtr_CheckPtr(&claimed);
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(claimed) + 0x48) = 1;
        void* citEnt = *reinterpret_cast<void**>(citizenIBP);
        fn_InternalBasePtr_CheckPtr(&claimed);
        *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(claimed) + 0x50) = citEnt;
    }

    // --- stamp the citizen ------------------------------------------------------
    const int32_t workTypeId = *reinterpret_cast<int32_t*>(S + 0x20);
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citEntity = *reinterpret_cast<uintptr_t*>(citizenIBP);

    // vtable slot 10 (+0x50) GetTarget: member value-return ABI -- RCX=this,
    // RDX=hidden retbuf (the exe reuses its own `self` home slot as the buffer).
    void* targetOut = nullptr;
    {
        typedef void* (*GetTarget_t)(void*, void*);
        auto getTarget = *reinterpret_cast<GetTarget_t*>(
            *reinterpret_cast<uintptr_t*>(S) + 0x50);
        void* ret = getTarget(self, &targetOut);
        targetOut = ret ? *reinterpret_cast<void**>(ret) : nullptr;
    }

    *reinterpret_cast<int32_t*>(citEntity + 0x30) = workTypeId;
    *reinterpret_cast<uint32_t*>(citEntity + 0x54) |= 4;
    *reinterpret_cast<uint32_t*>(citEntity + 0xb0) = 0;
    if (targetOut == *reinterpret_cast<void**>(citEntity + 0x1f0) && workTypeId >= 0) {
        *reinterpret_cast<uint32_t*>(citEntity + 0x88) |= 0x20000;
    }

    // first still set => nothing was dispatched => 0; else 2.
    if (first == 0) result = 2;
    s_x_memFree(buf);
    return result;
}

// ============================================================
// Pickup::Slot4 dispatcher.
//
// When s_fn_Slot4_Original resolves (address registered) the dispatcher calls it;
// otherwise the port runs — the address is unregistered and the old fallback
// silently returned 1 ("no work") forever. Set BANISHED_PICKUP_PORT=1 to force the
// port even when the original resolves, for A/B comparison against it.
// ============================================================
int Pickup_Slot4_Impl(Pickup* self, void* entityIBP, uint64_t param3, uint64_t param4) {
    (void)param4;  // exe reads only RCX/RDX/R8; param4 is not part of the ABI here
    static const bool forcePort = [] {
        const char* e = std::getenv("BANISHED_PICKUP_PORT");
        return e && e[0] && e[0] != '0';
    }();
    if (s_fn_Slot4_Original && !forcePort) {
        return s_fn_Slot4_Original(self, entityIBP, param3, param4);
    }
    return Pickup_Execute_Port(self, entityIBP, reinterpret_cast<void*>(param3));
}


// ============================================================
// Pickup::Execute (Slot5, 0x1400f38f0)
// UNIQUE to Pickup -- handles re-targeting when citizen changes
// workplace during an active pickup.
//
// Algorithm:
// 1. If status == 0, return false (no active work)
// 2. If citizenIBP is null, return false
// 3. Check if citizen's current workplace matches citizenIBP's workplace
// 4. If different: cancel old work, re-run Slot4 for both entities
// 5. Return false (keep executing)
// ============================================================
int Pickup_Execute_Impl(Pickup* self, void* entityIBP, void* param3) {
    // [diag] Did a laborer actually claim the Pickup work that RawMaterialComponent::Draw
    // posted for a harvest pile? BANISHED_RAWMAT_PICKUP_TRACE.
    {
        static const bool trace = [] { const char* e = std::getenv("BANISHED_RAWMAT_PICKUP_TRACE");
                                       return e && e[0] && e[0] != '0'; }();
        static int n = 0;
        if (trace && ++n <= 12) {
            fprintf(stderr, "[RawMatPickup] Pickup::Execute #%d self=%p status=%d citizen=%p\n",
                    n, (void*)self, (int)self->status, (void*)self->citizenIBP);
            fflush(stderr);
        }
    }

    // Check status != 0 and citizenIBP != null
    if (self->status == 0 || self->citizenIBP == nullptr) {
        return 0;
    }

    // Check citizenIBP (+0x50)
    fn_InternalBasePtr_CheckPtr(entityIBP);
    uintptr_t citizenEntity = *(uintptr_t*)(*(uintptr_t*)entityIBP + 0x1f0);

    fn_InternalBasePtr_CheckPtr(&self->citizenIBP);
    uintptr_t storedCitizen = (uintptr_t)self->citizenIBP;

    if (citizenEntity != *(uintptr_t*)(storedCitizen + 0x1f0)) {
        // Citizen changed workplace -- cancel old work and re-target
        fn_InternalBasePtr_CheckPtr(&self->citizenIBP);
        uintptr_t oldCitizen = (uintptr_t)self->citizenIBP;

        fn_InternalBasePtr_CheckPtr((void*)(oldCitizen + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(oldCitizen + 0x278) + 0x48));

        // Re-run Slot4 with the new citizen
        uintptr_t newCitizenRef = *(uintptr_t*)entityIBP;
        uintptr_t local_buf[1] = { newCitizenRef };

        // Call Slot4 via vtable (guard against freed self after CancelAllWork)
        uintptr_t vtable = *(uintptr_t*)self;
        if (vtable == 0) {
            return 0;  // self was freed during CancelAllWork
        }
        typedef int (*Slot4_fn)(void*, void*, void*);
        auto slot4 = *reinterpret_cast<Slot4_fn*>(vtable + 0x20);
        int result = slot4(self, local_buf, param3);

        if (result == 2) {
            return 1;  // Successfully re-targeted
        }

        // Try again with stored citizen
        uintptr_t storedBuf[1] = { storedCitizen };
        slot4(self, storedBuf, param3);
    }

    return 0;
}


// ============================================================
// Pickup::Slot6 / OnComplete (0x1400f2ec0)
// Resets citizenIBP to null, sets status to 2 (completing).
// Returns false.
// ============================================================
bool Pickup_Slot6_Impl(Pickup* self) {
    uintptr_t nullIBP = 0;
    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
    self->citizenIBP = (void*)nullIBP;
    self->status = 2;
    return false;
}


// ============================================================
// Pickup::Slot7 / OnCancel (0x1400f2e80)
// Resets citizenIBP to null, sets status to 0 (unassigned).
// ============================================================
int Pickup_Slot7_Impl(Pickup* self) {
    uintptr_t nullIBP = 0;
    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
    self->citizenIBP = (void*)nullIBP;
    self->status = 0;
    return 0;
}


// ============================================================
// Pickup::Slot8 / CancelAll (0x1400f39f0)
// If status == 1 (assigned), cancels the citizen's active work.
// ============================================================
int Pickup_Slot8_Impl(Pickup* self) {
    Pickup_ResolveFuncs();   // lazy-resolve — factory-bypassed Pickups (see Slot9 note)
    if (self->status == 1) {
        fn_InternalBasePtr_CheckPtr(&self->citizenIBP);
        uintptr_t citizenEntity = (uintptr_t)self->citizenIBP;

        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// Pickup::Slot9 / Validate (0x1400f2e20)
// Validates the assigned citizen entity.
// If invalid, resets citizenIBP to null and status to 0.
// Returns true if valid (or no citizen assigned).
// ============================================================
bool Pickup_Slot9_Impl(Pickup* self, void* param2) {
    // 🔑 LAZY-RESOLVE — REQUIRED on every entry point that touches an s_fn_* pointer.
    // Pickup_ResolveFuncs() is otherwise only called from Pickup_Factory/CreateInPlace, but
    // the Pickup works that matter here NEVER go through the factory: RawMaterialComponent::
    // Draw (0x1401291d0) MemAllocs 0x60, runs Work_Ctor25 and stamps the vtable by hand. So
    // s_fn_ValidateEntity stayed null and this slot called address 0 (crash: "EXEC of 0",
    // misreported as VCALL-of-0 because RCX happened to be the Pickup). Same pattern as
    // removenaturalresource.cpp's "lazy-resolve (exe-ctor-built action)".
    Pickup_ResolveFuncs();
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t citizenRef = (uintptr_t)self->citizenIBP;

    if (citizenRef == 0) {
        return true;  // No citizen assigned -- nothing to validate
    }

    // Validate entity via FUN_1400f7100
    uintptr_t entityIBP[1] = { citizenRef };
    bool valid = s_fn_ValidateEntity(selfAddr, param2, entityIBP, nullptr);

    if (!valid) {
        // Entity no longer valid -- reset
        uintptr_t nullIBP = 0;
        fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
        self->citizenIBP = (void*)nullIBP;
        self->status = 0;
        return false;
    }

    return true;
}


// ============================================================
// Pickup::Slot10 / GetTarget (0x1400f3a50)
// Returns the global entity from GameWorld+0xb0+0x30.
// This is a global target, not per-instance.
// ============================================================
void* Pickup_Slot10_Impl(Pickup* /*self*/, void* outPtr) {
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityList = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)outPtr = *(uintptr_t*)(entityList + 0x30);
    return outPtr;
}


// ============================================================
// Pickup::Slot11 / HasWorker (0x14002c000)
// Returns true if citizenIBP is non-null (worker assigned).
// 10 bytes, unhookable.
// ============================================================
bool Pickup_Slot11_Impl(Pickup* self) {
    return self->citizenIBP != nullptr;
}


// ============================================================
// Pickup::Slot1 / GetTypeIndex (0x14002bfb0)
// Returns type index constant from DAT_1401c6be8.
// Tiny, not hooked.
// ============================================================
uint64_t Pickup_Slot1_Impl(Pickup* /*self*/) {
    if (s_typeIndex_Pickup) {
        return *s_typeIndex_Pickup;
    }
    return 0;
}


// ============================================================
// Pickup::Slot2 / GetTypeId (0x14002bfa0)
// Returns Pickup type ID constant (TypeInfo_Pickup).
// Tiny, not hooked.
// ============================================================
uint32_t Pickup_Slot2_Impl(Pickup* /*self*/) {
    return s_typeId_Pickup;
}


// ============================================================
// Pickup::DtorBody
// Sets vtable to Pickup, then Work, calls Internal dtor.
// Simpler than HuntWork -- no BST tree to free (Pickup doesn't
// have a BST, just simple fields).
// ============================================================
void Pickup_DtorBody(Pickup* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to Pickup
    *(void**)selfAddr = s_vtable_Pickup;

    // Set vtable to Work (dtor chain)
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// Pickup::ScalarDtor (0x14002c010)
// Calls dtor body, then conditional free based on flags.
// ============================================================
void* Pickup_ScalarDtor_Impl(Pickup* self, uint32_t flags) {
    Pickup_ResolveFuncs();   // lazy-resolve — factory-bypassed Pickups (see Slot9 note)
    Pickup_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}


// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161810 (13 slots), size 0x60, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// NOTE: Pickup's methods are named by slot number (Pickup_SlotN_Impl):
//   slot6 OnComplete = Slot6, slot7 OnCancel = Slot7, slot8 Cleanup = Slot8,
//   slot9 Validate = Slot9, slot10 GetTarget = Slot10, slot11 GetPriority = Slot11.
//   slot4 Execute = Pickup_Execute_Impl (Pickup's unique Execute override).
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

// Constructor — port of Pickup_Ctor (0x1400f3c00): Object::Internal::Internal
// -> Work base fields -> Pickup vtable + zero Pickup fields.
static void* Pickup_Ctor_Impl(Pickup* self) {
    fn_ObjectInternal_Ctor(self);

    uintptr_t a = (uintptr_t)self;
    // Work base vtable (intermediate, immediately overwritten)
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x140161a80);
    *(void**)(a + 0x10) = nullptr;
    *(void**)(a + 0x18) = nullptr;
    *(int32_t*)(a + 0x20) = -1;
    *(int32_t*)(a + 0x24) = 0;
    *(int32_t*)(a + 0x28) = -1;
    *(int32_t*)(a + 0x2c) = 0;
    *(void**)(a + 0x30) = nullptr;
    *(void**)(a + 0x38) = nullptr;
    *(int32_t*)(a + 0x40) = 0;

    // Pickup vtable (our registered standalone table)
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x140161810);
    self->status = 0;             // +0x48
    self->citizenIBP = nullptr;   // +0x50
    self->candidateList = nullptr;// +0x58

    return self;
}

static uint32_t Pickup_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("Pickup");
    return c;
}

static void* g_Pickup_Vtable[13] = {
    (void*)&Pickup_ScalarDtor_Impl,  // +0x00 ScalarDtor
    (void*)&Work_Slot1,              // +0x08 slot1 (null)
    (void*)&Pickup_GetTypeId,        // +0x10 GetTypeId
    (void*)&Pickup_Serialize_Impl,   // +0x18 Serialize
    // 🔑 Slot order verified against the exe vtable @0x140161810: [4]=0x1400f2f00 (the REAL
    // Execute — the ~400-line "scan for nearby piles, dispatch PerformWork, set status/citizen"
    // body; Ghidra mislabels it Pickup_Serialize), [5]=0x1400f38f0 (the re-target helper, which
    // itself CALLS slot4 via *(vtable+0x20)). These two were swapped: the f38f0 port sat in
    // slot4 and slot5 was a stub, so the pickup job could never run — Execute saw status==0 and
    // returned immediately, which is precisely the case where retail does the whole job.
    (void*)&Pickup_Slot4_Impl,       // +0x20 Execute      (0x1400f2f00)
    (void*)&Pickup_Execute_Impl,     // +0x28 re-target    (0x1400f38f0)
    (void*)&Pickup_Slot6_Impl,       // +0x30 OnComplete
    (void*)&Pickup_Slot7_Impl,       // +0x38 OnCancel
    (void*)&Pickup_Slot8_Impl,       // +0x40 Cleanup
    (void*)&Pickup_Slot9_Impl,       // +0x48 Validate
    (void*)&Pickup_Slot10_Impl,      // +0x50 GetTarget
    (void*)&Pickup_Slot11_Impl,      // +0x58 GetPriority
    (void*)&Work_Slot12,             // +0x60 slot12 (void nop)
};

extern "C" void RegisterPickupVtable() {
    RegisterExeAddr(0x140161810, (uintptr_t)g_Pickup_Vtable);
}
extern "C" void* Pickup_CreateInPlace(void* mem) {
    Pickup_ResolveFuncs();
    return Pickup_Ctor_Impl((Pickup*)mem);
}
extern "C" void* Pickup_Factory(void) {
    Pickup_ResolveFuncs();
    void* p = Work_Alloc(0x60);
    return p ? Pickup_Ctor_Impl((Pickup*)p) : nullptr;
}
REGISTER_TYPE(Pickup, "Work", &Pickup_Factory, &Pickup_CreateInPlace);
