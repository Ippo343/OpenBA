// code/Game/Component/citizenai_gather.cpp
//
// Brick 3b handler wave 4 — faithful 1:1 ports of the two near-identical CitizenAIComponent
// resource-gathering state handlers + one private helper they share-by-shape:
//
//   0x14010bc90  CitizenAI_State_GatherResources   (state id 6)  -> dd30 + CheckWorkplaceValid
//                                                    + Storage/QueryCount/GetResourceCount
//                                                    + Position/2e20 + FindPath + ef40 transfer-loop
//                                                    + MeshInstance hide-render branch + 12c3f0
//   0x14010b3e0  CitizenAI_State_GatherResources2  (state id 7)  -> same, with the private
//                                                    Storage_GetResourceCount2 + a resource-slot
//                                                    selection loop (count matching RawMaterial
//                                                    slots, Random pick, transfer)
//   0x14012ab00  Storage_GetResourceCount2  (private to Gather2; closure = Entity_FindComponent_Storage
//                                            @0x140006270 (REG) + IsDerivedFrom only -> closure-safe)
//
// Both handlers make their state ENTERABLE by validating/walking-to the citizen workplace and
// (when a carry target is found) building the pickup path + transferring resources onto the carrier.
// ValidateAndEnterState (citizenai_candidatelist.cpp) dispatches them by ResolveAddr; returning 1
// commits the state. Transcribed from the Ghidra decompile + disassembly (FUN 0x14010bc90 /
// 0x14010b3e0), offset-by-offset.
//
// SELF CHAIN: param_1 = CitizenAIComponent. self = param_1+0x38 (sibling CitizenComponent IBP).
//   citizen = *self. citizen+0x238 = workplace entity, +0x10 = own entity IBP, +0x18 = description
//   (CitizenDescription, +0xd8 = gather-rate scalar), +0x278 = CharacterComponent (+0x68 =
//   ModelComponent transform, +0x158/+0x15c/+0x160 = world pos). Success epilogue writes the
//   action-state flags to *self (+0x54 |= 4, +0x30 = -1, +0xb0 = 0 then +0xb0 = stateNum) and
//   returns CONCAT71(cz>>8, 1). Cooldown-fail stamps Time(rate * mult) into the CitizenAIComponent
//   timer (Gather: param_1+0x70; Gather2: param_1+0x78) and returns ...&0xff00. The
//   found-but-empty branch (resource count 0) runs the MeshInstance "hide" render branch on the
//   workplace MeshInstance (mirrors citizenai_handlers2.cpp SeekHappiness hide-render).
//
// GameWorld singleton (g_pGameWorldSingleton @ DAT_1401c3dd8): +0x60 = Graphics::Scene,
//   +0xb0 = (sub @+0x30, +0x50 = town carry capacity), +0xd8 = Random, +0xf0 = entity-list pair,
//   +0xf8 = MapGrid (int* width at [0]).
//
// Registered exe sub-callees (resolved via ResolveAddr; bodies NOT duplicated):
//   0x14008dd30  CitizenAI_State_sub_dd30           (citizenai_findtarget.cpp)   [find carry target]
//   0x1401158d0  CitizenComponent_CheckWorkplaceValid (reg_component_gap_A.cpp)
//   0x140002f90  Entity_FindComponent_MeshInstance  (registered)
//   0x140002ed0  Entity_FindComponent_Position      (reg_work_base.cpp)
//   0x140006270  Entity_FindComponent_Storage       (registered)  [citizen+0x10 variant]
//   0x140008090  Entity_FindComponent_Storage       (reg_component_people.cpp)   [workplace variant]
//   0x140122e20  Entity_impl_2e20                    (reg_pf_road.cpp)  [resolve world pos]
//   0x140087fb0  Entity_impl_7fb0                    (reg_pf_road.cpp)  [workplace anchor cell]
//   0x14008ef40  Entity_impl_ef40                    (reg_component_gap_A.cpp)  [carrier slot iter]
//   0x1400c3470  FUN_1400c3470 NotifyEntityChanged   (reg_action_hunt)
//   0x140116540  FUN_140116540  town carry-capacity  (registered)
//   0x14012c3f0  FUN_14012c3f0  MeshInstance set-flag (registered)
//   0x14012cf10  StorageComponent_QueryCount         (registered)
//   0x14012a9d0  Storage_GetResourceCount            (reg_component_people.cpp)  [Gather]
//   0x14012ab00  Storage_GetResourceCount2           (THIS FILE)                 [Gather2]
//   0x1400bf790  Pathfinding_FindPath                (registered)
//   0x1400ca230  Resource_TransferToCarrier          (registered)
//   0x140115e60  CitizenAI_State_LongTermPlan        (another agent registers; null-guarded)
//
// KEY ABI (verified): Entity_impl_ef40 (0x14008ef40) IGNORES its first arg — real sig
//   (ignoredArg1, int64_t* out, void* entityIBP, int category). Mirror the decompile but arg1
//   is don't-care (the decompile passes the citizen there; harmless).
//
// SELF-CONTAINED-BRICK type-ids (UNFILLED 0xFFFFFFFF in standalone; resolved via
//   Object::GetTypeIdByName, every scan-result deref null-guarded):
//     DWORD_1401c7190  -> "CitizenDescription"     [cooldown-rate IsDerivedFrom on citizen+0x18]
//     DWORD_1401c7cb8  -> "RawMaterialDescription"  [resource-slot child-scan IsDerivedFrom]
//
// .rdata float consts (read from the exe):
//     DAT_140172234 = 300.0f  (found-but-empty cooldown multiplier)
//     DAT_1401721fc =  75.0f  (no-valid-workplace cooldown multiplier)
//
// The MeshInstance/AddObject/RemoveObject render sub-branch touches the still-INERT ModelComponent
// — every deref through it is null-guarded so it stays crash-safe (it is NOT exercised at
// world-start). This file must NOT depend on hooks.h — it is portable for the standalone exe.

#ifdef STANDALONE_EXE

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// --- registered exe sub-callees ---
typedef int64_t* (*fn_dd30_t)(int64_t*, int64_t*, uint32_t, uint32_t, int64_t*, float*, uint32_t); // 0x14008dd30
typedef char     (*fn_checkWork_t)(void* citizen, void* workplaceSlot);                  // 0x1401158d0
typedef void*    (*fn_findComp_t)(int64_t entity, void* outIBP);                         // 0x140002f90 / 0x140002ed0 / 0x140006270 / 0x140008090
typedef void     (*fn_2e20_t)(int64_t comp, float* outVec3, int, int, void*);            // 0x140122e20
typedef uint32_t (*fn_7fb0_t)(void* ibp, float* outVec3orNull);                          // 0x140087fb0
typedef void     (*fn_ef40_t)(int64_t ignored, int64_t* out, void* entityIBP, int cat);  // 0x14008ef40
typedef void     (*fn_c3470_t)(void* internal);                                          // 0x1400c3470
typedef int      (*fn_116540_t)(int64_t citizen);                                        // 0x140116540
typedef void     (*fn_12c3f0_t)(int64_t* mesh, int mode, char flag);                     // 0x14012c3f0
typedef int      (*fn_queryCount_t)(int64_t storageWrap);                                // 0x14012cf10
typedef int      (*fn_getCount_t)(int64_t storageWrap);                                  // 0x14012a9d0
typedef void     (*fn_transfer_t)(int64_t carrier, int64_t storage, int one);            // 0x1400ca230
typedef void     (*fn_findPath_t)(void* outResult, void* citizenPtr, int pathType,
                                  void* destEntityIBP, float* destPos, void* srcEntityIBP,
                                  float* srcPos, long sourceEntity, int itemCount, int flags,
                                  int p11, char p12, int p13, int p14);                   // 0x1400bf790
typedef uint64_t (*fn_ltp_t)(int64_t citizen);                                           // 0x140115e60

static fn_dd30_t       s_dd30       = nullptr;
static fn_checkWork_t  s_checkWork  = nullptr;
static fn_findComp_t   s_findMesh   = nullptr;  // 0x140002f90
static fn_findComp_t   s_findPos    = nullptr;  // 0x140002ed0
static fn_findComp_t   s_findStor1  = nullptr;  // 0x140006270 (citizen+0x10)
static fn_findComp_t   s_findStor2  = nullptr;  // 0x140008090 (workplace)
static fn_2e20_t       s_2e20       = nullptr;
static fn_7fb0_t       s_7fb0       = nullptr;
static fn_ef40_t       s_ef40       = nullptr;
static fn_c3470_t      s_c3470      = nullptr;
static fn_116540_t     s_116540     = nullptr;
static fn_12c3f0_t     s_12c3f0     = nullptr;
static fn_queryCount_t s_queryCount = nullptr;
static fn_getCount_t   s_getCount   = nullptr;
static fn_transfer_t   s_transfer   = nullptr;
static fn_findPath_t   s_findPath   = nullptr;
static fn_ltp_t        s_ltp        = nullptr;
static int             (*s_getCount2)(int64_t) = nullptr;  // Storage_GetResourceCount2 (this file)

static uint32_t s_tid_CitizenDescription     = 0;  // DWORD_1401c7190
static uint32_t s_tid_RawMaterialDescription = 0;  // DWORD_1401c7cb8

static const float kMultEmpty   = 300.0f;  // DAT_140172234
static const float kMultInvalid = 75.0f;   // DAT_1401721fc

// fwd-decl of the helper (registered separately)
} // namespace

extern "C" int Storage_GetResourceCount2(int64_t param_1);

namespace {

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_dd30       = reinterpret_cast<fn_dd30_t>(ResolveAddr(Module::Exe, 0x14008dd30));
    s_checkWork  = reinterpret_cast<fn_checkWork_t>(ResolveAddr(Module::Exe, 0x1401158d0));
    s_findMesh   = reinterpret_cast<fn_findComp_t>(ResolveAddr(Module::Exe, 0x140002f90));
    s_findPos    = reinterpret_cast<fn_findComp_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_findStor1  = reinterpret_cast<fn_findComp_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_findStor2  = reinterpret_cast<fn_findComp_t>(ResolveAddr(Module::Exe, 0x140008090));
    s_2e20       = reinterpret_cast<fn_2e20_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_7fb0       = reinterpret_cast<fn_7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_ef40       = reinterpret_cast<fn_ef40_t>(ResolveAddr(Module::Exe, 0x14008ef40));
    s_c3470      = reinterpret_cast<fn_c3470_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    s_116540     = reinterpret_cast<fn_116540_t>(ResolveAddr(Module::Exe, 0x140116540));
    s_12c3f0     = reinterpret_cast<fn_12c3f0_t>(ResolveAddr(Module::Exe, 0x14012c3f0));
    s_queryCount = reinterpret_cast<fn_queryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    s_getCount   = reinterpret_cast<fn_getCount_t>(ResolveAddr(Module::Exe, 0x14012a9d0));
    s_transfer   = reinterpret_cast<fn_transfer_t>(ResolveAddr(Module::Exe, 0x1400ca230));
    s_findPath   = reinterpret_cast<fn_findPath_t>(ResolveAddr(Module::Exe, 0x1400bf790));
    s_ltp        = reinterpret_cast<fn_ltp_t>(ResolveAddr(Module::Exe, 0x140115e60));
    s_getCount2  = &Storage_GetResourceCount2;
    if (s_tid_CitizenDescription == 0)     s_tid_CitizenDescription     = ::Object::GetTypeIdByName("CitizenDescription");
    if (s_tid_RawMaterialDescription == 0) s_tid_RawMaterialDescription = ::Object::GetTypeIdByName("RawMaterialDescription");
    s_init = true;
}

static inline void CheckPtr(void* p) { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
static inline uintptr_t GW() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }
static inline int* MapGrid() { return *reinterpret_cast<int**>(GW() + 0xf8); }

static inline bool TraceOn() {
    static int s_ts = -1;
    if (s_ts < 0) { const char* e = getenv("BANISHED_CITIZENAI_TRACE"); s_ts = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s_ts == 1;
}

// citizen ModelComponent transform (+0x68 of CharacterComponent at citizen+0x278).
static int64_t CitizenModel(int64_t citizen) {
    CheckPtr(reinterpret_cast<void*>(citizen + 0x278));
    int64_t ch = *reinterpret_cast<int64_t*>(citizen + 0x278);
    if (ch == 0) return 0;
    CheckPtr(reinterpret_cast<void*>(ch + 0x68));
    return *reinterpret_cast<int64_t*>(ch + 0x68);
}

// model world pos (+0x158/+0x15c) -> clamped grid cell (w*row + col).
static uint32_t ModelCell(int64_t model, int* mapGrid) {
    int w = *mapGrid;
    int col = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) + static_cast<float>(w >> 1));
    int row = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) + static_cast<float>(w >> 1));
    if (col < 0) col = 0; if (w <= col) col = w - 1;
    if (row < 0) row = 0; if (w <= row) row = w - 1;
    return static_cast<uint32_t>(w * row + col);
}

// cooldown rate scalar: ( (IsDerivedFrom(citizen+0x18, CitizenDescription) ? desc : 0)+0xd8 ) * mult.
// Original reads *(0+0xd8) when the IsDerivedFrom check fails — guard to 0 to stay crash-safe.
static int64_t CooldownStamp(int64_t citizen, float mult) {
    void* desc = *reinterpret_cast<void**>(citizen + 0x18);
    bool ok = (desc && fn_IsDerivedFrom && fn_IsDerivedFrom(desc, s_tid_CitizenDescription));
    float rate = ok ? *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0xd8) : 0.0f;
    int64_t stamp = 0;
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&stamp, rate * mult);
    return stamp;
}

// The shared MeshInstance "hide-render" branch (decompile: found==0 + workplace storage empty).
// Mirrors citizenai_handlers2.cpp SeekHappiness hide-render. flagBit distinguishes Gather (0x40)
// from Gather2 (0x80) — the OR into *(plVar3+0x1e) (== mi+0xf0). Every deref null-guarded.
static void HideRenderWorkplaceMesh(int64_t* self, fn_findComp_t findStor, int (*getCount)(int64_t),
                                    uint32_t flagBit) {
    CheckPtr(self);
    int64_t citizen = *self;
    if (*reinterpret_cast<int64_t*>(citizen + 0x238) == 0) return;

    CheckPtr(self);
    int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
    CheckPtr(&wp);
    int64_t storBuf[2] = {0, 0};
    void* sp = findStor ? findStor(wp, storBuf) : nullptr;
    CheckPtr(sp);
    int64_t storComp = sp ? *reinterpret_cast<int64_t*>(sp) : 0;
    int cnt = (storComp && getCount) ? getCount(storComp) : 0;
    if (cnt != 0) return;  // not empty -> nothing to hide

    CheckPtr(self);
    int64_t wp2 = *reinterpret_cast<int64_t*>(*self + 0x238);
    CheckPtr(&wp2);
    int64_t meshBuf[2] = {0, 0};
    void* mp = s_findMesh ? s_findMesh(wp2, meshBuf) : nullptr;
    CheckPtr(mp);
    if (!mp) return;
    int64_t mi = *reinterpret_cast<int64_t*>(mp);  // plVar3 (MeshInstance object)
    if (mi == 0) return;
    CheckPtr(reinterpret_cast<void*>(mi + 0x10));  // plVar3+2
    int64_t inner = *reinterpret_cast<int64_t*>(mi + 0x10);
    // gated: ((inner+0x40 >> 10) & 1) == 0
    if (inner == 0 || (*reinterpret_cast<uint32_t*>(inner + 0x40) >> 10 & 1) != 0) return;

    // OR [mi+0xf0], flagBit  (== *(plVar3+0x1e) |= 0x40 / 0x80)
    *reinterpret_cast<uint32_t*>(mi + 0xf0) |= flagBit;
    uint32_t changed;
    uint8_t* gw = reinterpret_cast<uint8_t*>(GW());
    if (*reinterpret_cast<int*>(mi + 0xf0) == 0) {
        // mask empty -> remove from scene
        if (-1 < *reinterpret_cast<int32_t*>(mi + 0x100)) {
            if (fn_SceneRemoveObject)
                fn_SceneRemoveObject(*reinterpret_cast<void**>(gw + 0x60),
                                     *reinterpret_cast<uint32_t*>(mi + 0x100));
            *reinterpret_cast<int32_t*>(mi + 0x100) = static_cast<int32_t>(0xffffffff);
        }
        uint32_t before = *reinterpret_cast<uint32_t*>(mi + 0x20);
        uint32_t after = before & 0xfffffffb;
        *reinterpret_cast<uint32_t*>(mi + 0x20) = after;
        changed = after ^ before;
    } else {
        // mask non-empty -> ensure in scene + run vtable+0x40 update
        int32_t* settings = g_pGameSettingsPtr ? *g_pGameSettingsPtr : nullptr;
        if (settings && reinterpret_cast<char*>(settings)[0xc] != '\0' &&
            *reinterpret_cast<int32_t*>(mi + 0x100) < 0) {
            uint32_t id = fn_Graphics_Scene_AddObject
                ? fn_Graphics_Scene_AddObject(*reinterpret_cast<void**>(gw + 0x60),
                                              reinterpret_cast<void*>(mi + 0x28))
                : 0xffffffffu;
            *reinterpret_cast<uint32_t*>(mi + 0x100) = id;
        }
        int64_t t0 = 0;
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t0, 0.0f);
        typedef void (*slot40_t)(int64_t, int64_t*);
        (*reinterpret_cast<slot40_t*>(*reinterpret_cast<int64_t*>(mi) + 0x40))(mi, &t0);
        uint32_t before = *reinterpret_cast<uint32_t*>(mi + 0x20);
        uint32_t after = before | 4;
        *reinterpret_cast<uint32_t*>(mi + 0x20) = after;
        changed = after ^ before;
    }
    if ((changed & 0xffff) != 0 && *reinterpret_cast<int64_t*>(mi + 0x10) != 0) {
        CheckPtr(reinterpret_cast<void*>(mi + 0x10));
        int64_t notifyTarget = *reinterpret_cast<int64_t*>(mi + 0x10);
        if (s_c3470) s_c3470(reinterpret_cast<void*>(notifyTarget));
    }
}

// Scan an entity's child list (slots at entity+0x28, count at entity+0x30) for the first child
// whose vtable+0x10 type id == RawMaterialDescription, IsDerivedFrom-confirmed. Returns the child
// (or null). Mirrors the inner do/while in the decompile.
static void* FindRawMaterialChild(int64_t entity) {
    if (entity == 0) return nullptr;
    int count = *reinterpret_cast<int*>(entity + 0x30);
    int64_t slots = *reinterpret_cast<int64_t*>(entity + 0x28);
    for (int i = 0; i < count; ++i) {
        int64_t child = *reinterpret_cast<int64_t*>(slots + (int64_t)i * 8);
        if (child == 0) continue;
        typedef uint32_t (*typeId_t)(int64_t);
        uint32_t tid = (*reinterpret_cast<typeId_t*>(*reinterpret_cast<int64_t*>(child) + 0x10))(child);
        if (tid == s_tid_RawMaterialDescription) {
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(reinterpret_cast<void*>(child), s_tid_RawMaterialDescription))
                return reinterpret_cast<void*>(child);
            return nullptr;
        }
    }
    return nullptr;
}

} // namespace

// ============================================================
// 0x14012ab00 — Storage_GetResourceCount2  (private helper for Gather2)
//   Walks the storage component's row entries (entity+0x10 -> Storage; rows at +0x28, count +0x30,
//   stride 0x10). For each row finds the RawMaterialDescription child; if its +0x5c bit6 set, adds
//   (row+8 - row+0xc) to the running count. Returns the total. Closure = Entity_FindComponent_Storage
//   + IsDerivedFrom only -> closure-safe.
// ============================================================
extern "C" int Storage_GetResourceCount2(int64_t param_1) {
    EnsureInit();
    CheckPtr(reinterpret_cast<void*>(param_1 + 0x10));
    int64_t storBuf[2] = {0, 0};
    void* sp = s_findStor1 ? s_findStor1(*reinterpret_cast<int64_t*>(param_1 + 0x10), storBuf) : nullptr;
    CheckPtr(storBuf);
    int64_t stor = storBuf[0];
    int total = 0;
    if (stor == 0) return 0;
    int count = *reinterpret_cast<int*>(stor + 0x30);
    int64_t byteOff = 0;
    for (int i = 0; i < count; ++i) {
        CheckPtr(storBuf);
        int64_t rows = *reinterpret_cast<int64_t*>(stor + 0x28);
        int64_t row = *reinterpret_cast<int64_t*>(rows + byteOff);
        void* rm = FindRawMaterialChild(row);
        if (rm && (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rm) + 0x5c) >> 6 & 1) != 0) {
            total += *reinterpret_cast<int*>(rows + 8 + byteOff) - *reinterpret_cast<int*>(rows + 0xc + byteOff);
        }
        byteOff += 0x10;
    }
    return total;
}

// Gather (state 6) carrier transfer loop (decompile tail of the found!=0 branch). Walks the
// carrier-slot list produced by ef40, transferring while the citizen carry-capacity (capLeft)
// holds. ef40's entityIBP arg is &found (the carry-target, NOT the workplace); its first arg is
// IGNORED. Mirror the decompile's chaining: first call from the citizen `*self`, subsequent calls
// from the prior path-result entity (pathResult).
static void GatherTransferLoop(int64_t* self, int64_t pathResult, int64_t found, int capLeft) {
    int64_t citizen = *self;
    int64_t entIBP = found;                  // &found is the ef40 entityIBP arg (RBP+0x6f)
    int64_t carrierSlot = 0;                 // ef40 out (RBP+0x77 / local_res18)
    if (s_ef40) s_ef40(citizen, &carrierSlot, &entIBP, 0);
    while (carrierSlot != 0) {
        void* rm = FindRawMaterialChild(carrierSlot);
        int slotCost = rm ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rm) + 0x48) : 0x7fffffff;
        if (capLeft < slotCost) break;
        CheckPtr(&pathResult);
        if (s_transfer) s_transfer(pathResult, carrierSlot, 1);
        capLeft -= slotCost;
        entIBP = found;
        if (s_ef40) s_ef40(pathResult, &carrierSlot, &entIBP, 0);
    }
}

// Gather2 (state 7) resource-slot SELECTION + transfer (decompile LAB_14010b92f .. LAB_14010baf1 +
// LAB_14010bb8a). `found` = the carry-target entity; pathResult = the FindPath result entity (the
// carrier handle for Resource_TransferToCarrier). Outer loop re-scans `found`'s Storage each pass:
//   pass 1 — count rows whose RawMaterialDescription child has +0x5c bit6 set AND (row+8 - row+0xc)
//            > 0; track a preferred index (last match whose rm+0x58 byte != 0), else stays -1.
//   if any match: if no preferred, Random-pick chosen = Rand(matchCount).
//   pass 2 — walk to the chosen-th match; transfer it if capLeft >= rm+0x48; deduct and re-loop.
// Returns when a pass yields no transfer. Every scan-deref null-guarded.
static void SelectAndTransfer(int64_t found, int64_t pathResult, int capLeft) {
    for (;;) {
        // Storage(found) -> stor
        int64_t storWrap = found;
        CheckPtr(&storWrap);
        int64_t storBuf[2] = {0, 0};
        if (s_findStor1) s_findStor1(storWrap, storBuf);
        CheckPtr(storBuf);
        int64_t stor = storBuf[0];
        if (stor == 0) return;

        int rowCount = *reinterpret_cast<int*>(stor + 0x30);   // local_60
        int chosenIdx = -1;                                     // local_res8 low
        int matchCount = 0;                                    // pvVar21

        // pass 1: count matches + preferred index
        int64_t rowsBase = 0;  // local_res10 (= *(stor+0x28))
        for (int cur = 0; cur < rowCount; ++cur) {
            CheckPtr(&storBuf[0]);
            rowsBase = *reinterpret_cast<int64_t*>(stor + 0x28);
            int64_t cursor = (int64_t)cur * 0x10;
            int64_t child = *reinterpret_cast<int64_t*>(rowsBase + cursor);
            void* rm = FindRawMaterialChild(child);
            bool flagged = rm && (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rm) + 0x5c) >> 6 & 1) != 0;
            int avail = *reinterpret_cast<int*>(rowsBase + cursor + 8) - *reinterpret_cast<int*>(rowsBase + cursor + 0xc);
            if (flagged && avail > 0) {
                if (*reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(rm) + 0x58) != '\0')
                    chosenIdx = matchCount;
                ++matchCount;
            }
        }

        if (matchCount == 0) return;
        if (chosenIdx < 0) {
            void* rng = *reinterpret_cast<void**>(GW() + 0xd8);
            chosenIdx = (rng && fn_Random_Rand) ? (int)fn_Random_Rand(rng, (uint32_t)matchCount) : 0;
        }

        // pass 2: find the chosenIdx-th match
        int matchCounter = 0;   // local_res10 low
        int64_t hitChild = 0;   // the row child to transfer (lVar5)
        int hitCursor = -1;
        for (int cur = 0; cur < rowCount; ++cur) {
            CheckPtr(&storBuf[0]);
            int64_t rb = *reinterpret_cast<int64_t*>(stor + 0x28);   // local_88
            int64_t cursor = (int64_t)cur * 0x10;
            int64_t child = *reinterpret_cast<int64_t*>(rb + cursor);
            void* rm = FindRawMaterialChild(child);
            bool flagged = rm && (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rm) + 0x5c) >> 6 & 1) != 0;
            int avail = *reinterpret_cast<int*>(rb + cursor + 8) - *reinterpret_cast<int*>(rb + cursor + 0xc);
            if (flagged && avail > 0) {
                if (matchCounter == chosenIdx) { hitChild = child; hitCursor = cur; break; }
                ++matchCounter;
            }
        }

        if (hitChild == 0 || hitCursor < 0) return;   // LAB_14010bb8a: lVar5 == 0 -> done

        // LAB_14010bb8a: resolve the chosen row child's RawMaterial cost, transfer if capLeft allows
        void* rm = FindRawMaterialChild(hitChild);
        int cost = rm ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rm) + 0x48) : 0x7fffffff;
        if (capLeft < cost) return;
        CheckPtr(&pathResult);
        if (s_transfer) s_transfer(pathResult, hitChild, 1);
        capLeft -= cost;
        // goto LAB_14010b92f: re-loop with reduced capacity
    }
}

// ============================================================
// Shared "found != 0" path-and-transfer body for the Gather variant (state 6).
//   1) MeshInstance set-flag(stateNum) on the workplace
//   2) Position -> 2e20 -> destPos; FindPath(citizen -> workplace storage)
//   3) compute capLeft = max(town-capacity, FUN_140116540(citizen))
//   4) transfer loop
//   5) set citizen action-state flags + return 1
// ============================================================

// ============================================================
// 0x14010bc90 — CitizenAI_State_GatherResources  (state id 6)
// ============================================================
extern "C" uint64_t CitizenAI_State_GatherResources(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    CheckPtr(self);

    bool haveWorkplace = (*reinterpret_cast<int64_t*>(*self + 0x238) != 0);
    char valid = 0;
    if (haveWorkplace) {
        CheckPtr(self);
        int64_t workplace = *reinterpret_cast<int64_t*>(*self + 0x238);  // local_res8
        CheckPtr(self);
        valid = s_checkWork ? s_checkWork(reinterpret_cast<void*>(*self), &workplace) : 0;
    }

    if (haveWorkplace && valid != 0) {
        // own-storage QueryCount: Storage(citizen+0x10) -> QueryCount
        CheckPtr(self);
        int64_t citizen = *self;
        CheckPtr(reinterpret_cast<void*>(citizen + 0x10));
        int64_t storBuf[2] = {0, 0};
        if (s_findStor1) s_findStor1(*reinterpret_cast<int64_t*>(citizen + 0x10), storBuf);
        CheckPtr(storBuf);
        int qc = (storBuf[0] && s_queryCount) ? s_queryCount(storBuf[0]) : 0;
        if (qc > 0) {
            CheckPtr(self);
            uint64_t r = s_ltp ? s_ltp(*self) : 0;
            *reinterpret_cast<int32_t*>(param_1 + 0x2c) = 6;
            if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather] self=%p LTP path (state6)\n",(void*)param_1);fflush(stderr);n++;} }
            return (r & 0xffffffffffffff00ULL) | 1;
        }

        // compute the citizen grid cell
        CheckPtr(self);
        citizen = *self;
        int64_t model = CitizenModel(citizen);
        int* mg = MapGrid();
        int w = *mg;
        uint32_t cell = ModelCell(model, mg);

        // bGoal = workplace ? 7fb0(&wp) : -1
        CheckPtr(self);
        uint32_t bGoal;
        if (*reinterpret_cast<int64_t*>(*self + 0x238) == 0) {
            bGoal = 0xffffffff;
        } else {
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            bGoal = s_7fb0 ? s_7fb0(&wp, nullptr) : 0xffffffff;
        }

        // dd30 find carry target
        float dest[4] = {0, 0, 0, 0};   // local_70
        int64_t found = 0;              // local_res10
        int64_t outComp = 0;           // local_res18
        int64_t* listpair = *reinterpret_cast<int64_t**>(GW() + 0xf0);
        if (s_dd30) s_dd30(listpair, &found, cell, bGoal, &outComp, dest, 0);
        (void)w;

        CheckPtr(self);
        if (found == 0) {
            // ---- cooldown + hide-render branch ----
            int64_t cz = *self;
            int64_t stamp = CooldownStamp(cz, kMultEmpty);
            *reinterpret_cast<int64_t*>(param_1 + 0x70) = stamp;
            CheckPtr(self);
            *reinterpret_cast<uint32_t*>(*self + 0x88) |= 4;
            HideRenderWorkplaceMesh(self, s_findStor2, s_getCount, 0x40);
            if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather] self=%p no target (cooldown+hide)\n",(void*)param_1);fflush(stderr);n++;} }
            return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
        }

        // ---- found != 0: build path + transfer ----
        // 1) MeshInstance set-flag(6) on workplace
        CheckPtr(self);
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            CheckPtr(self);
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            CheckPtr(&wp);
            int64_t meshBuf[2] = {0, 0};
            void* mp = s_findMesh ? s_findMesh(wp, meshBuf) : nullptr;
            CheckPtr(mp);
            if (mp && s_12c3f0) s_12c3f0(*reinterpret_cast<int64_t**>(mp), 6, '\0');
        }

        // 2) Position(workplace) -> 2e20 -> destPos; FindPath
        CheckPtr(self);
        int64_t wpForPos = *reinterpret_cast<int64_t*>(*self + 0x238);
        CheckPtr(&wpForPos);
        int64_t posBuf[2] = {0, 0};
        void* pp = s_findPos ? s_findPos(wpForPos, posBuf) : nullptr;
        CheckPtr(pp);
        float destPos[4] = {0, 0, 0, 0};  // local_60
        if (pp && s_2e20) s_2e20(*reinterpret_cast<int64_t*>(pp), destPos, 0, 0, nullptr);

        CheckPtr(self);
        int64_t citizenVal = *self;                                     // local_88
        int64_t destEntityIBP = *reinterpret_cast<int64_t*>(citizenVal + 0x238);  // local_res18
        int64_t pathResult = 0;                                          // local_res20
        if (s_findPath)
            s_findPath(&pathResult, &citizenVal, -1, &destEntityIBP, destPos,
                       &found, dest, 0, -1, 0, 1, '\x01', 1, 0);

        // 3) capLeft = max( town-capacity, FUN_140116540(citizen) )
        int townCap = *reinterpret_cast<int*>(
            *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(GW() + 0xb0) + 0x30) + 0x50);
        CheckPtr(self);
        int citCap = s_116540 ? s_116540(*self) : 0;
        int capLeft = (townCap < citCap) ? citCap : townCap;

        // 4) transfer loop (ef40 walks carrier slots of `found`, the carry target)
        GatherTransferLoop(self, pathResult, found, capLeft);

        // 5) set citizen action-state flags + return 1
        CheckPtr(self);
        int64_t cz = *self;
        *reinterpret_cast<uint32_t*>(cz + 0x54) |= 4;
        *reinterpret_cast<int32_t*>(cz + 0x30) = static_cast<int32_t>(0xffffffff);
        *reinterpret_cast<int32_t*>(cz + 0xb0) = 0;
        CheckPtr(self);
        cz = *self;
        *reinterpret_cast<int32_t*>(cz + 0xb0) = 1;
        if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather] self=%p ENTERED (transfer)\n",(void*)param_1);fflush(stderr);n++;} }
        return (static_cast<uint64_t>(cz) & 0xffffffffffffff00ULL) | 1;
    }

    // ---- no valid workplace: cooldown(*0xd8 * 75.0) -> param_1+0x70 ----
    CheckPtr(self);
    int64_t citizen = *self;
    int64_t stamp = CooldownStamp(citizen, kMultInvalid);
    *reinterpret_cast<int64_t*>(param_1 + 0x70) = stamp;
    if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather] self=%p no-workplace cooldown\n",(void*)param_1);fflush(stderr);n++;} }
    return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
}

// ============================================================
// 0x14010b3e0 — CitizenAI_State_GatherResources2  (state id 7)
// ============================================================
extern "C" uint64_t CitizenAI_State_GatherResources2(int64_t param_1) {
    EnsureInit();
    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);
    CheckPtr(self);

    bool haveWorkplace = (*reinterpret_cast<int64_t*>(*self + 0x238) != 0);
    char valid = 0;
    if (haveWorkplace) {
        CheckPtr(self);
        int64_t workplace = *reinterpret_cast<int64_t*>(*self + 0x238);
        CheckPtr(self);
        valid = s_checkWork ? s_checkWork(reinterpret_cast<void*>(*self), &workplace) : 0;
    }

    if (haveWorkplace && valid != 0) {
        // own-storage QueryCount
        CheckPtr(self);
        int64_t citizen = *self;
        CheckPtr(reinterpret_cast<void*>(citizen + 0x10));
        int64_t storBuf[2] = {0, 0};
        if (s_findStor1) s_findStor1(*reinterpret_cast<int64_t*>(citizen + 0x10), storBuf);
        CheckPtr(storBuf);
        int qc = (storBuf[0] && s_queryCount) ? s_queryCount(storBuf[0]) : 0;
        if (qc > 0) {
            CheckPtr(self);
            uint64_t r = s_ltp ? s_ltp(*self) : 0;
            *reinterpret_cast<int32_t*>(param_1 + 0x2c) = 7;
            if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather2] self=%p LTP path (state7)\n",(void*)param_1);fflush(stderr);n++;} }
            return (r & 0xffffffffffffff00ULL) | 1;
        }

        // compute cell
        CheckPtr(self);
        citizen = *self;
        int64_t model = CitizenModel(citizen);
        int* mg = MapGrid();
        int w = *mg;
        uint32_t cell = ModelCell(model, mg);

        // bGoal = workplace ? 7fb0(&wp) : -1
        CheckPtr(self);
        uint32_t bGoal;
        if (*reinterpret_cast<int64_t*>(*self + 0x238) == 0) {
            bGoal = 0xffffffff;
        } else {
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            bGoal = s_7fb0 ? s_7fb0(&wp, nullptr) : 0xffffffff;
        }

        // dd30 find carry target (category arg = 1 here, vs 0 in Gather)
        float srcPos[4] = {0, 0, 0, 0};  // local_88
        int64_t found = 0;               // local_70
        int64_t outComp = 0;             // local_res10
        int64_t* listpair = *reinterpret_cast<int64_t**>(GW() + 0xf0);
        if (s_dd30) s_dd30(listpair, &found, cell, bGoal, &outComp, srcPos, 1);
        (void)w;

        if (found == 0) {
            // ---- cooldown + hide-render branch ----
            CheckPtr(self);
            *reinterpret_cast<uint32_t*>(*self + 0x88) |= 4;
            CheckPtr(self);
            int64_t cz = *self;
            int64_t stamp = CooldownStamp(cz, kMultEmpty);
            *reinterpret_cast<int64_t*>(param_1 + 0x78) = stamp;
            HideRenderWorkplaceMesh(self, s_findStor2, s_getCount2, 0x80);
            if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather2] self=%p no target (cooldown+hide)\n",(void*)param_1);fflush(stderr);n++;} }
            return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
        }

        // ---- found != 0: MeshInstance flag(7), Position+2e20, FindPath ----
        CheckPtr(self);
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            CheckPtr(self);
            int64_t wp = *reinterpret_cast<int64_t*>(*self + 0x238);
            CheckPtr(&wp);
            int64_t meshBuf[2] = {0, 0};
            void* mp = s_findMesh ? s_findMesh(wp, meshBuf) : nullptr;
            CheckPtr(mp);
            if (mp && s_12c3f0) s_12c3f0(*reinterpret_cast<int64_t**>(mp), 7, '\0');
        }

        CheckPtr(self);
        int64_t wpForPos = *reinterpret_cast<int64_t*>(*self + 0x238);
        CheckPtr(&wpForPos);
        int64_t posBuf[2] = {0, 0};
        void* pp = s_findPos ? s_findPos(wpForPos, posBuf) : nullptr;
        CheckPtr(pp);
        float destPos[4] = {0, 0, 0, 0};  // local_98
        if (pp && s_2e20) s_2e20(*reinterpret_cast<int64_t*>(pp), destPos, 0, 0, nullptr);

        CheckPtr(self);
        int64_t citizenVal = *self;                                     // local_res20
        int64_t destEntityIBP = *reinterpret_cast<int64_t*>(citizenVal + 0x238);  // local_res18
        int64_t pathResult = 0;                                          // local_78
        if (s_findPath)
            s_findPath(&pathResult, &citizenVal, -1, &destEntityIBP, destPos,
                       &found, srcPos, 0, -1, 0, 1, '\x01', 1, 0);

        // capLeft = max( town-capacity, FUN_140116540(citizen) )
        int townCap = *reinterpret_cast<int*>(
            *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(GW() + 0xb0) + 0x30) + 0x50);
        CheckPtr(self);
        int citCap = s_116540 ? s_116540(*self) : 0;
        int capLeft = (townCap < citCap) ? citCap : townCap;

        // Gather2's resource-slot SELECTION loop (decompile LAB_14010b92f .. LAB_14010baf1):
        //   pass 1 — count carrier rows whose RawMaterialDescription +0x5c bit6 set and have
        //            (row+8 - row+0xc) > 0; track a preferred index (rows whose +0x58 byte != 0).
        //   if any match: if the preferred index is unset, Random-pick one of the matches.
        //   pass 2 — walk to the chosen match and TransferToCarrier it.
        // Mirrors the decompile offset-by-offset; every scan-deref null-guarded. The carrier-slot
        // entity is found via ef40 (the loop's storage component is `found`'s Storage).
        SelectAndTransfer(found, pathResult, capLeft);

        // ---- set citizen action-state flags + return 1 ----
        CheckPtr(self);
        int64_t cz = *self;
        *reinterpret_cast<uint32_t*>(cz + 0x54) |= 4;
        *reinterpret_cast<int32_t*>(cz + 0x30) = static_cast<int32_t>(0xffffffff);
        *reinterpret_cast<int32_t*>(cz + 0xb0) = 0;
        CheckPtr(self);
        cz = *self;
        *reinterpret_cast<int32_t*>(cz + 0xb0) = 2;
        if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather2] self=%p ENTERED (transfer)\n",(void*)param_1);fflush(stderr);n++;} }
        return (static_cast<uint64_t>(cz) & 0xffffffffffffff00ULL) | 1;
    }

    // ---- no valid workplace: cooldown(*0xd8 * 75.0) -> param_1+0x78 ----
    CheckPtr(self);
    int64_t citizen = *self;
    int64_t stamp = CooldownStamp(citizen, kMultInvalid);
    *reinterpret_cast<int64_t*>(param_1 + 0x78) = stamp;
    if (TraceOn()) { static int n=0; if(n<6){fprintf(stderr,"[Gather2] self=%p no-workplace cooldown\n",(void*)param_1);fflush(stderr);n++;} }
    return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
}

#endif // STANDALONE_EXE
