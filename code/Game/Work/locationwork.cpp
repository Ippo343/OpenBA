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

#include "Work/LocationWork.h"
#include "Work/locationwork_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CitizenComponent.h"     // ENDGAME B.3.9 D5: CitizenComponent::TypeId()
#include "Component/InteractDescription.h"  // ENDGAME B.3.9 D5: InteractDescription::TypeId()

// ============================================================
// Local function pointers (resolved once by LocationWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with LW_ to avoid conflicts

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

// fn_MemFreeStatic
typedef void (*LW_MemFreeStatic_t)(void* ptr);


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

// Float constant: g_fUVScale at 0x14017214c
static float* s_fUVScale = nullptr;

// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: LocationWork_Ctor
// disassembles to `0x1400efc7d: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.

static bool s_locationwork_funcs_resolved = false;

// ============================================================
// Resolution — called once during hook installation
// ============================================================
bool LocationWork_ResolveFuncs() {
    if (s_locationwork_funcs_resolved) return true;

    // Work::Serialize

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

    // Float constant: g_fUVScale at 0x14017214c
    s_fUVScale = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x14017214c));

    // Vtable pointers for dtor

    // Verify critical pointers
    bool ok = s_fn_FindComp_Storage && s_fn_StorageQueryCount &&
              s_fn_CitizenAI_LongTermPlan && s_fn_FindComp_Position &&
              s_fn_FindWalkablePos && s_fn_FindWalkablePosSub &&
              s_fn_WalkTowards_Ctor && s_fn_ValidateEntity &&
              s_fn_CancelAllWork && s_fn_FindSubEntity &&
              fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && s_fUVScale;

    s_locationwork_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[LocationWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
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
        if (!s_fUVScale) printf("  MISSING: fUVScale\n");
    }
#endif

    return ok;
}


// ============================================================
// LocationWork::Serialize (0x1400efba0)
// Pattern: Work_Serialize + Serializer(v0) + InternalBasePtr::Serialize(+0x48)
//   + validate IsDerivedFrom, clear if invalid
// ============================================================
void LocationWork::Persist(void* stream) {
    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create serializer (version=0)
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x48
    reinterpret_cast<Object::InternalBasePtr*>(&_field48)->Serialize(*reinterpret_cast<IO::Serializer*>(serializerBuf));   // B.3.4 bridge

    // Validate: if _field48 is non-null, check IsDerivedFrom
    if (_field48 != nullptr) {
        if (!fn_IsDerivedFrom(_field48, CitizenComponent::TypeId())) {
            _field48 = nullptr;
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
int LocationWork::Execute(void* citizenIBP, void* /*context*/) {
    uintptr_t* selfQ = (uintptr_t*)this;

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

    // DAT_1401c7938 = InteractDescription (ENDGAME B.3.9 D5: by cell, not the legacy
    // variable name "RadiusDesc").
    bool isDerivedRadius = fn_IsDerivedFrom(descObj, InteractDescription::TypeId());
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

    uintptr_t workEntIBP[2] = {0};
    uintptr_t* result = static_cast<uintptr_t*>(GetTarget(workEntIBP));
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
int LocationWork::OnComplete(void* /*arg1*/, void* /*arg2*/, int /*arg3*/) {
    uintptr_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    _field48 = (void*)nullIBP;
    return false;
}


// ============================================================
// LocationWork::Slot7 (0x1400ef820) — clear target, void return
// Constructs null InternalBasePtr into +0x48.
// ============================================================
int LocationWork::OnCancel(void* /*arg1*/, void* /*arg2*/, int /*arg3*/) {
    uintptr_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    _field48 = (void*)nullIBP;
    return 0;
}


// ============================================================
// LocationWork::Slot8 (0x1400efaf0) — cancel single target's work
// If target at +0x48 exists, navigates to its AI subsystem and
// cancels all work items.
// ============================================================
int LocationWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;

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
int LocationWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t targetIBP = *(uintptr_t*)(selfAddr + 0x48);

    if (targetIBP == 0) {
        return true;  // No target — considered valid (nothing to invalidate)
    }

    // Validate the entity
    bool stillValid = s_fn_ValidateEntity(selfAddr, nullptr, &targetIBP, nullptr);

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
void* LocationWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

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
// Slot 5 is Work's (0x1400f7540, `XOR AL,AL; RET`): the old dead
// LocationWork_Execute_Impl copy of it is gone.
// ============================================================
// Slot12 (+0x60) — retail 0x1400efae0, this class's own `RET 0` (every other Work
// table shares 0x140006b10). The same behaviour; declared so the table matches.
void LocationWork::Slot12(int /*version*/) {}


// ============================================================
// LocationWork::Slot11 (0x1400295a0) — checks +0x48 != 0
// Returns true if target entity reference is non-null.
// Too small to hook (10 bytes).
// ============================================================
int LocationWork::GetPriority() {
    return _field48 != nullptr ? 1 : 0;
}


// ~LocationWork is defaulted: the LocationWork then Work stamps and `~Internal` are the
// compiler's chain — all retail's 0x14002a850 does before the free.


// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf field zeroed. The stamps are the compiler's. LocationWork_ResolveFuncs
// wires the file's helper pointers (the old factory did the same).
LocationWork::LocationWork() : Work(), _field48(nullptr) {
    LocationWork_ResolveFuncs();
}

LocationWork::LocationWork(void* pEntityIBP) : Work(pEntityIBP), _field48(nullptr) {
    LocationWork_ResolveFuncs();
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_LocationWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id, slot 12 the shared nop) and REGISTER_TYPE over LocationWork_Factory /
// _CreateInPlace (both Work_GenericCtor), plus two vtable-pointer statics for the
// dtor's stamps.

// create 0x14002a810: MemAlloc(sizeof, 8), then the ctor.
void* LocationWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(LocationWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) LocationWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* LocationWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) LocationWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(LocationWork, "Work")
