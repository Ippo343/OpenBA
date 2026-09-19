// code/Game/Work/locationwork.cpp
// Clean C++ reimplementation of LocationWork — all methods.
// Source: Game/Work/LocationWork.cpp (from profile build debug strings)
//
// LocationWork (0x50 bytes) — Location-based work action.
// Inherits: Object::Internal -> Work (0x48) -> LocationWork
// Vtable: 0x1401616a0
//
// Unlike HerbalistWork/ForesterWork which extend AutoPickupWork with a BST,
// LocationWork extends Work directly with a single InternalBasePtr at +0x48.
// This stores a reference to the citizen entity assigned to the work.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/locationwork_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Local function pointers (resolved once by LocationWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with LW_ to avoid conflicts

// Work::Serialize base
typedef void  (*LW_Work_Serialize_t)(void* self, void* stream);

// Entity_FindComponent_Storage (0x140006270)
typedef void* (*LW_FindComp_Storage_t)(uintptr_t entity, void* outIBP);
// StorageComponent_QueryCount (0x14012cf10)
typedef int   (*LW_StorageQueryCount_t)(void* storageComp);
// CitizenAI_State_LongTermPlan (0x140115e60)
typedef void  (*LW_CitizenAI_LongTermPlan_t)(void* citizenEntity);
// Entity_FindComponent_Position (0x140002ed0)
typedef void* (*LW_FindComp_Position_t)(uintptr_t entity, void* outIBP);

// FUN_140122be0 — FindWalkablePosition (workplace position helper)
typedef uint8_t (*LW_FindWalkablePos_t)(uintptr_t posComp, uintptr_t descOffset,
                                         float* srcPos, float* outPos, int* outDist);
// FUN_140122b00 — FindWalkablePosition variant (sub-position)
typedef uint8_t (*LW_FindWalkablePosSub_t)(uintptr_t entity, int index,
                                            float* srcPos, float* outPos, int* outDist);

// Pathfinding_WalkTowards action constructor (0x1400bb490)
// Creates a WalkTowards action: (citizenIBP, priority, position, flag1, fScale, flag2)
//
// ⚠ This read 0x1400f8620 until 2026-08-15 — a DIFFERENT function entirely
// (WorkGrid_TryAssignWorkplace, a 4-arg BST search over a work grid, registered under
// that name in standalone/reg_work_grid.cpp and used correctly by cleararea /
// fishingwork / pasturework). Calling it with WalkTowards' six arguments walked a
// "BST" out of the float destination array and faulted READ of -1.
//
// It never fired before because nothing in the world had ever CONSTRUCTED a
// LocationWork: the only producer is AttendanceComponent's state-2 enter, and that
// controller was inert until the promotion wave. The first live school posted a
// LocationWork, a citizen picked it up out of Idle, and Slot4 crashed a few seconds
// in. Classic latent leaf surfaced by a new feature — see
// reference_registered_but_never_executed.
typedef void (*LW_WalkTowards_Ctor_t)(void* citizenIBP, int priority,
                                       float* position, int flag1, float fScale, uint32_t flag2);

// FUN_1400f7100 — ValidateEntity
typedef bool (*LW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);

// FUN_1400c5400 — CancelAllWork
typedef void (*LW_CancelAllWork_t)(void* bstRoot);

// FUN_140002e10 — FindSubEntity
typedef void* (*LW_FindSubEntity_t)(uintptr_t entity, void* outIBP);

// Object::Internal::~Internal
typedef void (*LW_Internal_Dtor_t)(void* self);

// fn_MemFreeStatic
typedef void (*LW_MemFreeStatic_t)(void* ptr);

static LW_Work_Serialize_t             s_fn_Work_Serialize = nullptr;

static LW_FindComp_Storage_t           s_fn_FindComp_Storage = nullptr;
static LW_StorageQueryCount_t          s_fn_StorageQueryCount = nullptr;
static LW_CitizenAI_LongTermPlan_t     s_fn_CitizenAI_LongTermPlan = nullptr;
static LW_FindComp_Position_t          s_fn_FindComp_Position = nullptr;
static LW_FindWalkablePos_t            s_fn_FindWalkablePos = nullptr;
static LW_FindWalkablePosSub_t         s_fn_FindWalkablePosSub = nullptr;
static LW_WalkTowards_Ctor_t           s_fn_WalkTowards_Ctor = nullptr;
static LW_ValidateEntity_t             s_fn_ValidateEntity = nullptr;
static LW_CancelAllWork_t              s_fn_CancelAllWork = nullptr;
static LW_FindSubEntity_t              s_fn_FindSubEntity = nullptr;
static LW_MemFreeStatic_t              s_fn_MemFreeStatic = nullptr;

// TypeId for Serialize validation (DAT_1401c7158)
static uint32_t s_typeId_Validate = 0;

// TypeId for RadiusDescription check (DAT_1401c7938)
static uint32_t* s_typeId_RadiusDesc = nullptr;

// Float constant: g_fUVScale at 0x14017214c
static float* s_fUVScale = nullptr;

// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: LocationWork_Ctor
// disassembles to `0x1400efc7d: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.
static void** s_vtable_LocationWork = nullptr;  // 0x1401616a0
static void** s_vtable_Work = nullptr;           // 0x140161a80

static bool s_locationwork_funcs_resolved = false;

// ============================================================
// Resolution — called once during hook installation
// ============================================================
bool LocationWork_ResolveFuncs() {
    if (s_locationwork_funcs_resolved) return true;

    // Work::Serialize
    s_fn_Work_Serialize = reinterpret_cast<LW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    // Entity component finders
    s_fn_FindComp_Storage = reinterpret_cast<LW_FindComp_Storage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<LW_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_CitizenAI_LongTermPlan = reinterpret_cast<LW_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FindComp_Position = reinterpret_cast<LW_FindComp_Position_t>(
        ResolveAddr(Module::Exe, 0x140002ed0));

    // Pathfinding helpers
    s_fn_FindWalkablePos = reinterpret_cast<LW_FindWalkablePos_t>(
        ResolveAddr(Module::Exe, 0x140122be0));
    s_fn_FindWalkablePosSub = reinterpret_cast<LW_FindWalkablePosSub_t>(
        ResolveAddr(Module::Exe, 0x140122b00));

    // WalkTowards action constructor (see the typedef comment for the 0x1400f8620 mixup)
    s_fn_WalkTowards_Ctor = reinterpret_cast<LW_WalkTowards_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400bb490));

    // Validation and cleanup
    s_fn_ValidateEntity = reinterpret_cast<LW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_CancelAllWork = reinterpret_cast<LW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<LW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<LW_MemFreeStatic_t>(fn_MemFreeStatic);

    // TypeId for Serialize validation — read uint32 value at 0x1401c7158
    uintptr_t typeIdAddr = ResolveAddr(Module::Exe, 0x1401c7158);
    if (typeIdAddr)
        s_typeId_Validate = *reinterpret_cast<uint32_t*>(typeIdAddr);

    // TypeId for RadiusDescription check (DAT_1401c7938)
    s_typeId_RadiusDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7938));

    // Float constant: g_fUVScale at 0x14017214c
    s_fUVScale = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x14017214c));

    // Vtable pointers for dtor
    s_vtable_LocationWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401616a0));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161a80));

    // Verify critical pointers
    bool ok = s_fn_Work_Serialize &&
              s_fn_FindComp_Storage && s_fn_StorageQueryCount &&
              s_fn_CitizenAI_LongTermPlan && s_fn_FindComp_Position &&
              s_fn_FindWalkablePos && s_fn_FindWalkablePosSub &&
              s_fn_WalkTowards_Ctor && s_fn_ValidateEntity &&
              s_fn_CancelAllWork && s_fn_FindSubEntity &&
              fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && s_typeId_RadiusDesc && s_fUVScale &&
              s_vtable_LocationWork && s_vtable_Work;

    s_locationwork_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[LocationWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_FindComp_Storage) printf("  MISSING: FindComp_Storage\n");
        if (!s_fn_StorageQueryCount) printf("  MISSING: StorageQueryCount\n");
        if (!s_fn_CitizenAI_LongTermPlan) printf("  MISSING: CitizenAI_LongTermPlan\n");
        if (!s_fn_FindComp_Position) printf("  MISSING: FindComp_Position\n");
        if (!s_fn_FindWalkablePos) printf("  MISSING: FindWalkablePos\n");
        if (!s_fn_FindWalkablePosSub) printf("  MISSING: FindWalkablePosSub\n");
        if (!s_fn_WalkTowards_Ctor) printf("  MISSING: WalkTowards_Ctor\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_typeId_RadiusDesc) printf("  MISSING: typeId_RadiusDesc\n");
        if (!s_fUVScale) printf("  MISSING: fUVScale\n");
        if (!s_vtable_LocationWork) printf("  MISSING: vtable_LocationWork\n");
        if (!s_vtable_Work) printf("  MISSING: vtable_Work (0x140161a80)\n");
    }
#endif

    return ok;
}


// ============================================================
// LocationWork::Serialize (0x1400efba0)
// Pattern: Work_Serialize + Serializer(v0) + InternalBasePtr::Serialize(+0x48)
//   + validate IsDerivedFrom, clear if invalid
// ============================================================
void LocationWork_Serialize_Impl(LocationWork* self, void* stream) {
    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create serializer (version=0)
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x48
    reinterpret_cast<Object::InternalBasePtr*>(&self->_field48)->Serialize(*reinterpret_cast<IO::Serializer*>(serializerBuf));   // B.3.4 bridge

    // Validate: if _field48 is non-null, check IsDerivedFrom
    if (self->_field48 != nullptr) {
        if (!fn_IsDerivedFrom(self->_field48, s_typeId_Validate)) {
            self->_field48 = nullptr;
        }
    }

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// LocationWork::Slot4 (0x1400ef880) — pathfinding/location work logic
//
// Algorithm:
// 1. If already has target (+0x48 != 0), return 1 (already assigned)
// 2. Check citizen's storage — if carrying items, call LongTermPlan
// 3. Find position component on workplace entity
// 4. Get citizen's current position
// 5. Find walkable position near workplace (main + sub-positions)
// 6. If found, create WalkTowards action and assign work
// 7. Store citizen ref at +0x48
// ============================================================
int LocationWork_Slot4_Impl(LocationWork* self, void* citizenIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t* selfQ = (uintptr_t*)self;

    // Step 1: Already has target?
    if (selfQ[9] != 0) {  // field +0x48
        return 1;
    }

    // Step 2: Check citizen's storage — if carrying items, replan
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    uintptr_t storageIBP = 0;
    s_fn_FindComp_Storage(*(uintptr_t*)(citizenEntity + 0x10), &storageIBP);

    int itemCount = s_fn_StorageQueryCount((void*)storageIBP);
    if (itemCount > 0) {
        s_fn_CitizenAI_LongTermPlan(*(void**)citizenIBP);
    }

    // Step 3: Find position component on workplace entity (self+0x10)
    uintptr_t posCompIBP = 0;
    s_fn_FindComp_Position(selfQ[2], &posCompIBP);  // self->_field10

    // Step 4: Get citizen's current position
    uintptr_t citEnt = *(uintptr_t*)citizenIBP;

    uintptr_t aiComp = *(uintptr_t*)(citEnt + 0x278);

    uintptr_t posObj = *(uintptr_t*)(aiComp + 0x68);

    float srcX = *(float*)(posObj + 0x158);
    float srcY = *(float*)(posObj + 0x15c);
    float srcZ = *(float*)(posObj + 0x160);

    // Step 5: Find walkable positions
    // Get description from position component, check if RadiusDescription
    void* descObj = *(void**)(posCompIBP + 0x18);
    int bestDist = 0x7fffffff;

    bool isDerivedRadius = fn_IsDerivedFrom(descObj, *s_typeId_RadiusDesc);
    void* radiusDesc = nullptr;
    if (isDerivedRadius) {
        radiusDesc = descObj;
    }

    float bestPos[3] = {0.0f, 0.0f, 0.0f};
    float srcPosArr[3] = {srcX, srcY, srcZ};

    // Check main position (via posComp + description offset +0x28)
    uint8_t found = s_fn_FindWalkablePos(
        posCompIBP, (uintptr_t)radiusDesc + 0x28,
        srcPosArr, bestPos, &bestDist);

    // Also check sub-positions
    int subCount = *(int*)(posCompIBP + 0x30);
    if (subCount > 0) {
        uintptr_t subArray = *(uintptr_t*)(posCompIBP + 0x28);
        for (int i = 0; i < subCount; i++) {
            uintptr_t* subPtr = (uintptr_t*)(subArray + (uintptr_t)i * 8);
            uint8_t subFound = s_fn_FindWalkablePosSub(
                *subPtr, 0, srcPosArr, bestPos, &bestDist);
            found = found | subFound;
        }
    }

    if (!found) {
        return 0;  // No walkable position found
    }

    // Step 6: Create WalkTowards action and assign work
    uintptr_t citizenRef = *(uintptr_t*)citizenIBP;
    uintptr_t citizenIBPLocal = citizenRef;

    // The Ghidra decompilation shows the last two params come from stack;
    // the pattern is: (citizenIBP, priority, position, 0, g_fUVScale, flag&0xffffff00)
    s_fn_WalkTowards_Ctor(
        &citizenIBPLocal,
        (int)selfQ[4],          // priority from self+0x20
        bestPos,
        0,                       // flag1
        *s_fUVScale,            // UV scale constant
        0);                      // flag2 (cleared upper bits)

    int priority = (int)selfQ[4];

    // Get work entity via vtable slot +0x50 (Slot10)
    uintptr_t citEntPost = *(uintptr_t*)citizenIBP;

    uintptr_t vtable = *(uintptr_t*)self;
    typedef uintptr_t* (*GetWorkEntity_t)(LocationWork*, void*);
    auto getWorkEntity = *(GetWorkEntity_t*)(vtable + 0x50);
    uintptr_t workEntIBP[2] = {0};
    uintptr_t* result = getWorkEntity(self, workEntIBP);
    uintptr_t workEntity = *result;

    // Set citizen flags
    *(uint32_t*)(citEntPost + 0x54) |= 4;
    *(int32_t*)(citEntPost + 0x30) = priority;
    *(uint32_t*)(citEntPost + 0xb0) = 0;

    // If work entity matches citizen's home entity and priority >= 0, set owned flag
    if (workEntity == *(uintptr_t*)(citEntPost + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citEntPost + 0x88) |= 0x20000;
    }

    // Store citizen as target at +0x48
    selfQ[9] = *(uintptr_t*)citizenIBP;

    return 2;
}


// ============================================================
// LocationWork::Slot6 (0x1400ef850) — clear target, return false
// Constructs null InternalBasePtr into +0x48.
// ============================================================
bool LocationWork_Slot6_Impl(LocationWork* self) {
    uintptr_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    self->_field48 = (void*)nullIBP;
    return false;
}


// ============================================================
// LocationWork::Slot7 (0x1400ef820) — clear target, void return
// Constructs null InternalBasePtr into +0x48.
// ============================================================
int LocationWork_Slot7_Impl(LocationWork* self) {
    uintptr_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    self->_field48 = (void*)nullIBP;
    return 0;
}


// ============================================================
// LocationWork::Slot8 (0x1400efaf0) — cancel single target's work
// If target at +0x48 exists, navigates to its AI subsystem and
// cancels all work items.
// ============================================================
int LocationWork_Slot8_Impl(LocationWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (*(uintptr_t*)(selfAddr + 0x48) == 0) {
        return 0;
    }

    // CheckPtr on the target entity at +0x48
    uintptr_t targetEntity = *(uintptr_t*)(selfAddr + 0x48);

    // Navigate to citizen AI component (+0x278) then cancel work (+0x48)
    s_fn_CancelAllWork((void*)(*(uintptr_t*)(targetEntity + 0x278) + 0x48));
    return 0;
}


// ============================================================
// LocationWork::Slot9 (0x1400ef7c0) — validate target
// Checks if the single target entity is still valid.
// If invalid, clears +0x48 to null and returns false.
// Returns true if valid (or no target).
// ============================================================
bool LocationWork_Slot9_Impl(LocationWork* self, void* citizenIBP) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t targetIBP = *(uintptr_t*)(selfAddr + 0x48);

    if (targetIBP == 0) {
        return true;  // No target — considered valid (nothing to invalidate)
    }

    // Validate the entity
    bool stillValid = s_fn_ValidateEntity(selfAddr, citizenIBP, &targetIBP, nullptr);

    if (!stillValid) {
        // Clear the target — construct null InternalBasePtr
        uintptr_t nullIBP = 0;
        new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *(uintptr_t*)(selfAddr + 0x48) = nullIBP;
        return false;
    }

    return true;
}


// ============================================================
// LocationWork::Slot10 (0x1400efb40) — get work entity sub-object
// Returns entity from self->_field10's sub-object at +0x48.
// IDENTICAL to HuntWork/PastureWork Slot10.
// ============================================================
void* LocationWork_Slot10_Impl(LocationWork* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on self+0x10
    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    // Find sub-entity via FUN_140002e10
    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);

    // CheckPtr on the result

    // Read +0x48 from the resolved entity and store in output
    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// LocationWork::Execute (0x1400f7540) — 3 bytes: XOR AL,AL; RET
// Always returns false. Too small to hook.
// ============================================================
bool LocationWork_Execute_Impl(LocationWork* /*self*/) {
    return false;
}


// ============================================================
// LocationWork::Slot11 (0x1400295a0) — checks +0x48 != 0
// Returns true if target entity reference is non-null.
// Too small to hook (10 bytes).
// ============================================================
bool LocationWork_Slot11_Impl(LocationWork* self) {
    return self->_field48 != nullptr;
}


// ============================================================
// LocationWork::ScalarDtor (0x14002a850)
// Simpler than BST classes — just vtable chain + Internal dtor.
// Sets LocationWork vtable, then Work vtable, calls Internal dtor,
// then conditional free.
// ============================================================
void* LocationWork_ScalarDtor_Impl(LocationWork* self, uint32_t flags) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to LocationWork (cleanup phase)
    *(void**)selfAddr = s_vtable_LocationWork;

    // Set vtable to Work (base class cleanup)
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);

    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x1401616a0 (13 slots), size 0x50, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// FLAGS:
//   - slot0  ScalarDtor : LocationWork_ScalarDtor_Impl  (present)
//   - slot3  Serialize  : LocationWork_Serialize_Impl
//   - slot4  Execute    : named LocationWork_Slot4_Impl (the vtable +0x20 slot;
//                         NOTE LocationWork_Execute_Impl is the SEPARATE 3-byte
//                         exe stub 0x1400f7540, NOT the vtable Execute slot).
//   - slot6  OnComplete : named LocationWork_Slot6_Impl  (returns bool)
//   - slot7  OnCancel   : named LocationWork_Slot7_Impl
//   - slot8  Cleanup    : named LocationWork_Slot8_Impl
//   - slot9  Validate   : named LocationWork_Slot9_Impl  (returns bool)
//   - slot10 GetTarget  : named LocationWork_Slot10_Impl
//   - slot11 GetPriority: named LocationWork_Slot11_Impl  (returns bool)
//   - Ctor              : NO LocationWork_Ctor_Impl defined (exe ctor not noted
//                         as a C++ _Impl) -> factory cannot construct ->
//                         /*FLAG*/ returns null until a Ctor_Impl is written.
//   - ResolveFuncs      : LocationWork_ResolveFuncs() (does NOT resolve a vtable).
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t LocationWork_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("LocationWork");
    return c;
}

static void* g_LocationWork_Vtable[13] = {
    (void*)&LocationWork_ScalarDtor_Impl, // +0x00 ScalarDtor
    (void*)&Work_Slot1,                   // +0x08 slot1 (null)
    (void*)&LocationWork_GetTypeId,       // +0x10 GetTypeId
    (void*)&LocationWork_Serialize_Impl,  // +0x18 Serialize
    (void*)&LocationWork_Slot4_Impl,      // +0x20 Execute   (LocationWork_Slot4_Impl)
    (void*)&Work_Slot5,                   // +0x28 slot5 (returns 0)
    (void*)&LocationWork_Slot6_Impl,      // +0x30 OnComplete(LocationWork_Slot6_Impl)
    (void*)&LocationWork_Slot7_Impl,      // +0x38 OnCancel  (LocationWork_Slot7_Impl)
    (void*)&LocationWork_Slot8_Impl,      // +0x40 Cleanup   (LocationWork_Slot8_Impl)
    (void*)&LocationWork_Slot9_Impl,      // +0x48 Validate  (LocationWork_Slot9_Impl)
    (void*)&LocationWork_Slot10_Impl,     // +0x50 GetTarget (LocationWork_Slot10_Impl)
    (void*)&LocationWork_Slot11_Impl,     // +0x58 GetPriority(LocationWork_Slot11_Impl)
    (void*)&Work_Slot12,                  // +0x60 slot12 (void nop)
};

extern "C" void RegisterLocationWorkVtable() {
    RegisterExeAddr(0x1401616a0, (uintptr_t)g_LocationWork_Vtable);
}
extern "C" void* LocationWork_CreateInPlace(void* mem) {
    LocationWork_ResolveFuncs();
    return Work_GenericCtor(mem, 0x50, g_LocationWork_Vtable);
}
extern "C" void* LocationWork_Factory(void) {
    LocationWork_ResolveFuncs();
    void* p = Work_Alloc(0x50);
    return p ? Work_GenericCtor(p, 0x50, g_LocationWork_Vtable) : nullptr;
}
REGISTER_TYPE(LocationWork, "Work", &LocationWork_Factory, &LocationWork_CreateInPlace);
