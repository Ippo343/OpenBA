// code/Game/Work/tradework.cpp
// Clean C++ reimplementation of TradeWork -- all methods.
// Source: Game/Work/TradeWork.cpp (from profile build debug strings)
//
// TradeWork (0x58 bytes) -- Trade post work action.
// Inherits: Object::Internal -> Work (0x48) -> TradeWork
// Vtable: 0x140161a10
//
// BST node layout (extended, 0x30 bytes):
//   +0x00: key (InternalBasePtr, citizen entity)
//   +0x08: entity ref (trade target)
//   +0x10: quantity (int32)
//   +0x18: parent pointer
//   +0x20: left child
//   +0x28: right child
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/tradework_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Local function pointers (resolved once by TradeWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with TrW_ to avoid conflicts

// Work::Serialize
typedef void  (*TrW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor
typedef void* (*TrW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*TrW_Serializer_Dtor_t)(void* self);
// BST serialization (extended BST at 0x14002c8a0)
typedef void  (*TrW_BST_Serialize_t)(void* bstRoot, void* serializer);
// BST find-and-remove by key (TradeWork_impl_c690 at 0x14002c690)
typedef void  (*TrW_BSTFindRemove_t)(void* bstRoot, void* keyIBP);
// BST node remove (TradeWork_helper_5740)
typedef void  (*TrW_BSTRemove_t)(void* bstRoot, void* node);
// Cancel all work items
typedef void  (*TrW_CancelAllWork_t)(void* citizenAISub);
// FUN_1400f7100 -- validate work entity
typedef bool  (*TrW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// MemAlloc/MemFree
typedef void* (*TrW_MemAlloc_t)(int64_t size, int align);
typedef void  (*TrW_MemFree_t)(void* ptr);
typedef void  (*TrW_MemFreeStatic_t)(void* ptr);
// Tree node cleanup (recursive free, extended BST at 0x140014b00)
typedef void  (*TrW_TreeNodeCleanup_t)(void* bstRoot, void* node);
// FindSubEntity
typedef void* (*TrW_FindSubEntity_t)(uintptr_t entity, void* outIBP);

// Slot4 original exe function (too complex for clean reimpl, called directly)
typedef uint32_t (*TrW_Slot4_Original_t)(void* self, void* citizenIBP, void* param3);

static TrW_Work_Serialize_t          s_fn_Work_Serialize = nullptr;
static TrW_Serializer_Ctor_t         s_fn_Serializer_Ctor = nullptr;
static TrW_Serializer_Dtor_t         s_fn_Serializer_Dtor = nullptr;
static TrW_BST_Serialize_t           s_fn_BST_Serialize = nullptr;
static TrW_BSTFindRemove_t           s_fn_BSTFindRemove = nullptr;
static TrW_BSTRemove_t               s_fn_BSTRemove = nullptr;
static TrW_CancelAllWork_t           s_fn_CancelAllWork = nullptr;
static TrW_ValidateEntity_t          s_fn_ValidateEntity = nullptr;
static TrW_MemAlloc_t                s_fn_MemAlloc = nullptr;
static TrW_MemFree_t                 s_fn_MemFree = nullptr;
static TrW_MemFreeStatic_t           s_fn_MemFreeStatic = nullptr;
static TrW_TreeNodeCleanup_t         s_fn_TreeNodeCleanup = nullptr;
static TrW_FindSubEntity_t           s_fn_FindSubEntity = nullptr;

// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: TradeWork_Ctor
// disassembles to `0x1400f6dbd: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.
static void** s_vtable_TradeWork = nullptr;  // 0x140161a10
static void** s_vtable_Work = nullptr;        // 0x140161a80

static bool s_trade_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool TradeWork_ResolveFuncs() {
    if (s_trade_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_Work_Serialize = reinterpret_cast<TrW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_BST_Serialize = reinterpret_cast<TrW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14002c8a0));
    s_fn_BSTFindRemove = reinterpret_cast<TrW_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x14002c690));
    s_fn_BSTRemove = reinterpret_cast<TrW_BSTRemove_t>(
        ResolveAddr(Module::Exe, 0x140025740));
    s_fn_CancelAllWork = reinterpret_cast<TrW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_ValidateEntity = reinterpret_cast<TrW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TreeNodeCleanup = reinterpret_cast<TrW_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014b00));
    s_fn_FindSubEntity = reinterpret_cast<TrW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // Dispatch slots (indirect through .rdata)
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor) s_fn_Serializer_Ctor = *reinterpret_cast<TrW_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor) s_fn_Serializer_Dtor = *reinterpret_cast<TrW_Serializer_Dtor_t*>(iatSerDtor);
    // WRONG SLOTS (fixed 2026-08-21). These used to read exe dispatch slots
    // 0x140151460 / 0x140151468 as MemAlloc / MemFree. 0x140151460 is NOT MemAlloc —
    // it is Graphics::Geometry::GetTransform, verified from the disassembly at
    // 0x1400fe1b5 and already documented at code/Game/Player/placementtool.cpp:78.
    // So TW_PushEntry's array grow called GetTransform(size, align), which ran on
    // garbage operands and took an ACCESS_VIOLATION (READ of 0x94) the first time a
    // citizen actually executed a TradeWork. A resolved pointer that is a REAL but
    // WRONG function — the failure mode reference_locationwork_walktowards_wrong_addr
    // describes.
    //
    // The allocator this code wants is the STATIC form,
    // ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z (size, alignment), exe slot
    // 0x140152b60 — which is what retail calls at its own array-grow sites (e.g.
    // 0x140085d9f in TradeUI_Serialize). engine_funcs already binds it correctly as
    // fn_MemAllocStatic, and this file was ALREADY using its counterpart
    // fn_MemFreeStatic a few functions further down, so the pair is now consistent.
    s_fn_MemAlloc = reinterpret_cast<TrW_MemAlloc_t>(fn_MemAllocStatic);
    s_fn_MemFree  = reinterpret_cast<TrW_MemFree_t>(fn_MemFreeStatic);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<TrW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Vtable pointers for dtor
    s_vtable_TradeWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161a10));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161a80));

    // Verify critical pointers
    bool ok = s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_BST_Serialize && s_fn_BSTFindRemove && s_fn_BSTRemove &&
              s_fn_CancelAllWork && s_fn_ValidateEntity &&
              s_fn_MemAlloc && s_fn_MemFree &&
              s_fn_TreeNodeCleanup && s_fn_FindSubEntity &&
              s_vtable_TradeWork && s_vtable_Work;

    s_trade_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[TradeWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_BSTFindRemove) printf("  MISSING: BSTFindRemove\n");
        if (!s_fn_BSTRemove) printf("  MISSING: BSTRemove\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_MemAlloc) printf("  MISSING: MemAlloc\n");
        if (!s_fn_MemFree) printf("  MISSING: MemFree\n");
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: TreeNodeCleanup\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
    }
#endif

    return ok;
}


// ============================================================
// TradeWork::Slot4 -- Trade assignment (0x1400f5d40, Ghidra "TradeWork_Serialize")
// ============================================================
// Faithful offset-by-offset port of the exe trade-assignment logic. Structure:
//   0. capacity gate (workers vs WorkComponent capacity)              -> return 1
//   1. if the citizen already carries trade goods -> LongTermPlan     -> return 2
//   2. PASS 1: for every resource type, compute net = requested(demand BST @ trade
//      comp +0x60) - inFlight(self+0x48 BST) - storageAvail(building storage). Push
//      net>0 into the "import" list (local_c8), net<0 (abs) into the "export" list
//      (local_f8). Each list is a 16-byte-entry {u64 resType, int qty} dynamic array
//      grown x3 (faithful to the inlined push_back).
//   3. PASS 2 over the export list: find a source storage via FindStorage(0x14008d590),
//      build a fetch path (Pathfinding_FindPath), insert a NEGATIVE-qty record into the
//      self+0x48 BST (c710), -> return 2 on first assignment.
//   4. PASS 3 over the import list: same, gated by StorageComponent CanAccept(0x14012cd80),
//      POSITIVE-qty record -> return 2 on first assignment.
//   default -> return 1.
// All 12 exe callees are registered/reimplemented (closure verified). Not exercised by
// the autostart harness (needs a live trade post + trader); acceptance = build-green +
// offset fidelity.

// ---- Slot4-specific resolved exe pointers (all registered) ----
typedef void* (*TrW_FindComp2_t)(uintptr_t entity, void* outIBP);
typedef int   (*TrW_CalcCap_t)(int64_t workComp);
typedef int   (*TrW_QueryCnt_t)(void* storageComp);
typedef char  (*TrW_CanAccept_t)(int64_t storageComp, uint64_t resType, int qty);
typedef void  (*TrW_FindStorage_t)(void* resList, int64_t* out, int cell, uint64_t resType,
                                   int qty, char flag, void* outPos, void* outIBP);   // 0x14008d590
typedef void  (*TrW_EntityPos_t)(int64_t entity, void* outVec3, int a, uint32_t rnd, float* b);
typedef int   (*TrW_ChildSum_t)(int64_t entity, int a);
typedef void  (*TrW_FindPath_t)(void* outResult, void* citizenPtr, uint32_t pathType,
                                void* destEntity, float* pos1, void* srcEntity, float* pos2,
                                int64_t resType, int qty, int a, uint8_t b, char c, int d, uint8_t e);
typedef void  (*TrW_LongTermPlan_t)(void* citizenEntity);

static TrW_FindComp2_t    s_s4_FindTradeComp    = nullptr;  // 0x140015140 (TradeComponent)
static TrW_FindComp2_t    s_s4_FindStorageComp  = nullptr;  // 0x140006270
static TrW_FindComp2_t    s_s4_FindPositionComp = nullptr;  // 0x140002ed0
static TrW_CalcCap_t      s_s4_CalcCapacity     = nullptr;  // 0x14012e5c0
static TrW_QueryCnt_t     s_s4_QueryCount       = nullptr;  // 0x14012cf10
static TrW_CanAccept_t    s_s4_CanAccept        = nullptr;  // 0x14012cd80
static TrW_FindStorage_t  s_s4_FindStorage      = nullptr;  // 0x14008d590
static TrW_EntityPos_t    s_s4_EntityPos        = nullptr;  // 0x140122e20
static TrW_ChildSum_t     s_s4_ChildSum         = nullptr;  // 0x140123070
static TrW_FindPath_t     s_s4_FindPath         = nullptr;  // 0x1400bf790
static TrW_LongTermPlan_t s_s4_LongTermPlan     = nullptr;  // 0x140115e60
static const uint32_t*    s_s4_pTidRawMat       = nullptr;  // 0x1401c7cb8 RawMaterialDescription
static const uint32_t*    s_s4_pTidStorage      = nullptr;  // 0x1401c7e78 StorageComponent
static const uint32_t*    s_s4_pTidInteract     = nullptr;  // 0x1401c7938 InteractDescription
static bool               s_s4_resolved         = false;

// The already-ported AVL insert (code/Game/Work/tradework_serialize.cpp), registered at
// 0x14002c710. Called with (tree=&self+0x48, out, record{key,extptr,int}).
extern "C" void* TradeWork_Insert_c710(int64_t* tree, void** out, int64_t rec);

static void TradeWork_Slot4_Resolve() {
    if (s_s4_resolved) return;
    s_s4_FindTradeComp    = reinterpret_cast<TrW_FindComp2_t>(ResolveAddr(Module::Exe, 0x140015140));
    s_s4_FindStorageComp  = reinterpret_cast<TrW_FindComp2_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_s4_FindPositionComp = reinterpret_cast<TrW_FindComp2_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_s4_CalcCapacity     = reinterpret_cast<TrW_CalcCap_t>(ResolveAddr(Module::Exe, 0x14012e5c0));
    s_s4_QueryCount       = reinterpret_cast<TrW_QueryCnt_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    s_s4_CanAccept        = reinterpret_cast<TrW_CanAccept_t>(ResolveAddr(Module::Exe, 0x14012cd80));
    s_s4_FindStorage      = reinterpret_cast<TrW_FindStorage_t>(ResolveAddr(Module::Exe, 0x14008d590));
    s_s4_EntityPos        = reinterpret_cast<TrW_EntityPos_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_s4_ChildSum         = reinterpret_cast<TrW_ChildSum_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_s4_FindPath         = reinterpret_cast<TrW_FindPath_t>(ResolveAddr(Module::Exe, 0x1400bf790));
    s_s4_LongTermPlan     = reinterpret_cast<TrW_LongTermPlan_t>(ResolveAddr(Module::Exe, 0x140115e60));
    s_s4_pTidRawMat       = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_s4_pTidStorage      = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    s_s4_pTidInteract     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7938));
    s_s4_resolved = true;
}

namespace {

inline uint32_t TW_Tid(const uint32_t* p) { return p ? *p : 0; }  // deref .rdata type-id, 0 if unfilled

// 16-byte list entry: {u64 resType, int qty, int pad}. Matches the exe stack record.
struct TWEntry { uint64_t key; int32_t qty; int32_t pad; };

// Scan a Description/StorageComponent inner list at obj+0x28 (array) / obj+0x30 (count) for
// an element whose GetTypeId (vtable +0x10) == tid, IsDerivedFrom-checked. Returns comp ptr.
void* TW_ScanDescComp(uint64_t obj, uint32_t tid) {
    if (!tid || !obj) return nullptr;
    int n = *reinterpret_cast<int*>(obj + 0x30);
    int64_t arr = *reinterpret_cast<int64_t*>(obj + 0x28);
    for (int i = 0; i < n; i++) {
        void* c = *reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<int64_t*>(c) + 0x10))(c);
        if (t == tid)
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tid)) ? c : nullptr;
    }
    return nullptr;
}

// Scan an entity's component list at ent+0x30 (array) / ent+0x38 (count).
void* TW_ScanEntityComp(int64_t ent, uint32_t tid) {
    if (!tid || !ent) return nullptr;
    int n = *reinterpret_cast<int*>(ent + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(ent + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
        void* c = *slot;
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<int64_t*>(c) + 0x10))(c);
        if (t == tid) {
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tid)) ? c : nullptr;
        }
    }
    return nullptr;
}

// Clamp a world coordinate (mapSize/2 + f) into [0, mapSize-1].
inline int TW_ClampCell(float f, int mapSize) {
    int v = static_cast<int>(static_cast<float>(mapSize >> 1) + f);
    if (v < 0) v = 0;
    if (v >= mapSize) v = mapSize - 1;
    return v;
}

// StorageComponent item-pair available = pair[+8] - pair[+0xc]; pairs are 16 bytes, key @ +0.
inline int TW_PairAvail(int64_t pairsBase, int64_t k) {
    return *reinterpret_cast<int*>(pairsBase + k * 0x10 + 8) -
           *reinterpret_cast<int*>(pairsBase + k * 0x10 + 0xc);
}

}  // namespace

// Faithful port of the inlined push_back for the import/export lists (16-byte entries,
// growth factor 3). Mirrors the exe: realloc only when capacity < count; on realloc copy
// the old entries then append; when in-place, just append; free the old buffer on realloc.
static void TW_PushEntry(TWEntry** arr, uint32_t* count, uint32_t* cap, uint64_t key, int32_t qty) {
    uint32_t oldCount = *count;
    uint32_t newCount = oldCount + 1;
    TWEntry* oldArr = *arr;
    TWEntry* dst = oldArr;
    if (static_cast<int>(*cap) < static_cast<int>(newCount)) {
        uint32_t newCap = (oldCount != 0) ? newCount * 3 : newCount;
        *cap = newCap;
        dst = reinterpret_cast<TWEntry*>(
            s_fn_MemAlloc ? s_fn_MemAlloc(static_cast<int64_t>(static_cast<int>(newCap)) << 4, 8) : nullptr);
    }
    uint32_t start = (oldArr == dst) ? oldCount : 0;
    if (dst) {
        for (uint32_t i = start; i < newCount; i++)
            dst[i] = (i < oldCount) ? oldArr[i] : TWEntry{key, qty, 0};
    }
    if (oldArr != dst && oldArr && s_fn_MemFree) s_fn_MemFree(oldArr);
    *arr = dst;
    *count = newCount;
}

// Shared assignment tail (both passes): read taskId (self+0x20), call Slot10 (vtable +0x50)
// to resolve the target entity, write the citizen's task-state fields, and insert a BST
// record {citizen, resType, signedQty} at self+0x48. Returns 2. `outBuf` is the scratch the
// preceding Pathfinding_FindPath wrote into (reused as the Slot10 out-IBP, as in the exe).
static int TW_CommitAssign(TradeWork* self, void* param2, uint64_t resType, int signedQty,
                           uint64_t* outBuf) {
    int64_t selfA = reinterpret_cast<int64_t>(self);
    int taskId = *reinterpret_cast<int*>(selfA + 0x20);            // plVar31[4] low dword

    int64_t citizen = *reinterpret_cast<int64_t*>(param2);

    typedef void* (*Slot10_t)(void* self, void* outIBP);
    Slot10_t slot10 = *reinterpret_cast<Slot10_t*>(*reinterpret_cast<int64_t*>(self) + 0x50);
    void* r = slot10(self, outBuf);
    int64_t slot10Ent = *reinterpret_cast<int64_t*>(r);

    *reinterpret_cast<int*>(citizen + 0x30) = taskId;
    *reinterpret_cast<uint32_t*>(citizen + 0x54) |= 4;
    *reinterpret_cast<uint32_t*>(citizen + 0xb0) = 0;
    if (slot10Ent == *reinterpret_cast<int64_t*>(citizen + 0x1f0) && taskId >= 0)
        *reinterpret_cast<uint32_t*>(citizen + 0x88) |= 0x20000;

    uint64_t rec[3];
    rec[0] = static_cast<uint64_t>(citizen);                       // +0x00 key (citizen entity)
    rec[1] = resType;                                             // +0x08 (resource type)
    rec[2] = static_cast<uint64_t>(static_cast<uint32_t>(signedQty)); // +0x10 int qty
    void* out = nullptr;
    TradeWork_Insert_c710(reinterpret_cast<int64_t*>(selfA + 0x48), &out, reinterpret_cast<int64_t>(&rec[0]));
    return 2;
}

int TradeWork_Slot4_Impl(TradeWork* self, void* citizenIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    TradeWork_ResolveFuncs();
    TradeWork_Slot4_Resolve();

    int64_t selfA = reinterpret_cast<int64_t>(self);
    void* param2 = citizenIBP;                       // local_res10 (citizen IBP)
    uint64_t gw = g_pGameWorldSingleton ? reinterpret_cast<uint64_t>(*g_pGameWorldSingleton) : 0;

    int ret = 1;
    TWEntry* arrA = nullptr; uint32_t cntA = 0, capA = 0;   // import list (net > 0)  -> local_c8/local_c0
    TWEntry* arrB = nullptr; uint32_t cntB = 0, capB = 0;   // export list (net < 0)  -> local_f8/local_f0
    uint64_t outBuf[8] = {0};                               // FindPath / Slot10 scratch (&local_res8)

    // ---- 0. capacity gate ----------------------------------------------------
    int workerCount = static_cast<int>(*reinterpret_cast<int64_t*>(selfA + 0x50));  // param_1[10]
    int64_t workComp = *reinterpret_cast<int64_t*>(selfA + 0x18);                    // param_1[3]
    if (*reinterpret_cast<int*>(workComp + 0x30) == 0 && s_s4_CalcCapacity)
        *reinterpret_cast<int*>(workComp + 0x30) = s_s4_CalcCapacity(workComp);
    if (*reinterpret_cast<int*>(workComp + 0x30) <= workerCount)
        return 1;

    // ---- find components -----------------------------------------------------
    int64_t tradeEntIBP = selfA + 0x10;                   // param_1+2 (&trade-post entity IBP)
    uint64_t tradeCompIBP[2] = {0};                       // local_58 (TradeComponent)
    if (s_s4_FindTradeComp) s_s4_FindTradeComp(*reinterpret_cast<uintptr_t*>(tradeEntIBP), tradeCompIBP);
    uint64_t bldgStorIBP[2] = {0};                        // local_50 (building storage)
    if (s_s4_FindStorageComp) s_s4_FindStorageComp(*reinterpret_cast<uintptr_t*>(tradeEntIBP), bldgStorIBP);
    uint64_t posCompIBP[2] = {0};                         // local_e0 (position)
    if (s_s4_FindPositionComp) s_s4_FindPositionComp(*reinterpret_cast<uintptr_t*>(tradeEntIBP), posCompIBP);

    int64_t bldgStor  = static_cast<int64_t>(bldgStorIBP[0]);   // local_78
    int64_t tradeComp = static_cast<int64_t>(tradeCompIBP[0]);
    int64_t posComp   = static_cast<int64_t>(posCompIBP[0]);    // local_e0 value
    uint64_t resList  = gw ? *reinterpret_cast<uint64_t*>(gw + 0xf0) : 0;   // pplVar30 / local_70

    // ---- 1. citizen already carrying trade goods? ---------------------------
    int64_t citizenEnt = *reinterpret_cast<int64_t*>(param2);
    uint64_t carryStorIBP[2] = {0};                       // local_res20
    if (s_s4_FindStorageComp) s_s4_FindStorageComp(*reinterpret_cast<uintptr_t*>(citizenEnt + 0x10), carryStorIBP);
    if (s_s4_QueryCount && s_s4_QueryCount(reinterpret_cast<void*>(carryStorIBP[0])) > 0) {
        if (s_s4_LongTermPlan) s_s4_LongTermPlan(reinterpret_cast<void*>(citizenEnt));
        return 2;
    }

    uint32_t rawMatTid   = TW_Tid(s_s4_pTidRawMat);
    uint32_t storageTid  = TW_Tid(s_s4_pTidStorage);
    uint32_t interactTid = TW_Tid(s_s4_pTidInteract);

    // ---- 2. PASS 1: build import/export lists over all resource types --------
    int resCount = resList ? *reinterpret_cast<int*>(resList + 0x118) : 0;   // pplVar30[0x23]
    int64_t resArr = resList ? *reinterpret_cast<int64_t*>(resList + 0x110) : 0; // pplVar30[0x22]
    for (int i = 0; i < resCount; i++) {
        uint64_t resType = *reinterpret_cast<uint64_t*>(resArr + static_cast<int64_t>(i) * 8);

        // (existence probe of the RawMaterialDescription component; result discarded, as in exe)
        TW_ScanDescComp(resType, rawMatTid);

        // requested count from the TradeComponent demand BST (tradeComp+0x60):
        //   node: +0x00 key, +0x08 value, +0x18 left, +0x20 right
        int requested = 0;
        if (tradeComp) {
            uint64_t* node = *reinterpret_cast<uint64_t**>(tradeComp + 0x60);
            while (node) {
                if (resType < node[0])       node = reinterpret_cast<uint64_t*>(node[3]);   // left  +0x18
                else if (resType <= node[0]) { requested = static_cast<int>(node[1]); break; } // value +0x08
                else                          node = reinterpret_cast<uint64_t*>(node[4]);   // right +0x20
            }
        }

        // storage available in the building storage comp (bldgStor): pairs at +0x28/+0x30.
        int storageAvail = 0;
        if (bldgStor && *reinterpret_cast<int*>(bldgStor + 0x30) > 0) {
            int64_t pairs = *reinterpret_cast<int64_t*>(bldgStor + 0x28);
            int pn = *reinterpret_cast<int*>(bldgStor + 0x30);
            for (int64_t k = 0; k < pn; k++) {
                if (*reinterpret_cast<uint64_t*>(pairs + k * 0x10) == resType) {
                    storageAvail = TW_PairAvail(pairs, k);
                    break;
                }
            }
        }

        // in-flight quantity from the self+0x48 extended BST (+0x20 left, +0x28 right,
        // +0x18 parent, key @ +0x08, qty @ +0x10) via in-order traversal.
        int inFlight = 0;
        int64_t root = *reinterpret_cast<int64_t*>(selfA + 0x48);
        if (root) {
            int64_t node = root, prev;
            do { prev = node; node = *reinterpret_cast<int64_t*>(node + 0x20); } while (node);
            node = prev;
            while (node) {
                if (*reinterpret_cast<uint64_t*>(node + 8) == resType)
                    inFlight += *reinterpret_cast<int*>(node + 0x10);
                int64_t right = *reinterpret_cast<int64_t*>(node + 0x28);
                if (right) {
                    node = right;
                    while (*reinterpret_cast<int64_t*>(node + 0x20)) node = *reinterpret_cast<int64_t*>(node + 0x20);
                } else {
                    int64_t parent = *reinterpret_cast<int64_t*>(node + 0x18);
                    while (parent && *reinterpret_cast<int64_t*>(parent + 0x28) == node) {
                        node = parent; parent = *reinterpret_cast<int64_t*>(parent + 0x18);
                    }
                    node = parent;
                }
            }
        }

        int net = (requested - inFlight) - storageAvail;
        if (net > 0)      TW_PushEntry(&arrA, &cntA, &capA, resType, net);       // import
        else if (net < 0) TW_PushEntry(&arrB, &cntB, &capB, resType, -net);      // export
    }

    // ---- 3. PASS 2 over the export list (arrB) -------------------------------
    for (uint32_t bi = 0; bi < cntB; bi++) {
        uint64_t resType = arrB[bi].key;
        void* matDesc = TW_ScanDescComp(resType, rawMatTid);          // pvVar24
        int qty = arrB[bi].qty;

        // clamp qty to the citizen's carry-storage free space:
        int64_t citizen = *reinterpret_cast<int64_t*>(param2);
        int64_t carryEnt = *reinterpret_cast<int64_t*>(citizen + 0x10);
        void* carryStor = TW_ScanEntityComp(carryEnt, storageTid);    // pvVar18
        if (matDesc && *reinterpret_cast<int*>(reinterpret_cast<int64_t>(matDesc) + 0x48) != 0) {
            int freeSpace = *reinterpret_cast<int*>(reinterpret_cast<int64_t>(carryStor) + 0x38);
            int cur = s_s4_QueryCount ? s_s4_QueryCount(carryStor) : 0;
            int room = (freeSpace - cur) / *reinterpret_cast<int*>(reinterpret_cast<int64_t>(matDesc) + 0x48);
            if (room < qty) qty = room;
        }

        // building storage comp -> its item pairs, find resType:
        int64_t bldgEnt = *reinterpret_cast<int64_t*>(tradeEntIBP);
        void* bldgStorComp = TW_ScanEntityComp(bldgEnt, storageTid);  // -> local_e8
        int64_t e8 = reinterpret_cast<int64_t>(bldgStorComp);
        if (!e8 || *reinterpret_cast<int*>(e8 + 0x30) <= 0) continue;
        int64_t pairs = *reinterpret_cast<int64_t*>(e8 + 0x28);
        int pn = *reinterpret_cast<int*>(e8 + 0x30);
        int matchK = -1;
        for (int64_t k = 0; k < pn; k++) {
            if (*reinterpret_cast<uint64_t*>(pairs + k * 0x10) == resType) { matchK = static_cast<int>(k); break; }
        }
        if (matchK < 0) continue;
        int avail = TW_PairAvail(pairs, matchK);
        if (avail == 0) continue;
        if (avail < qty) qty = avail;

        // fetch source cell from the citizen AI sub-object (citizen+0x278 -> +0x68):
        int64_t c2 = *reinterpret_cast<int64_t*>(param2);
        int64_t ai = *reinterpret_cast<int64_t*>(c2 + 0x278);
        int64_t nav = *reinterpret_cast<int64_t*>(ai + 0x68);
        int mapSize = gw ? **reinterpret_cast<int**>(gw + 0xf8) : 0;
        int cellX = TW_ClampCell(*reinterpret_cast<float*>(nav + 0x158), mapSize);
        int cellY = TW_ClampCell(*reinterpret_cast<float*>(nav + 0x15c), mapSize);
        int cell = mapSize * cellY + cellX;

        uint64_t searchPos[2] = {0};                       // local_90 (out from FindStorage)
        uint64_t srcStorIBP[2] = {0};                      // local_d8 (out IBP from FindStorage)
        int64_t srcStorPrimary = 0;                        // local_b8 (primary result)
        if (s_s4_FindStorage)
            s_s4_FindStorage(reinterpret_cast<void*>(resList), &srcStorPrimary, cell, resType, qty,
                             1, searchPos, srcStorIBP);
        if (srcStorPrimary == 0) { continue; }

        // destination position from the trade post position comp:
        uint64_t destPos[2] = {0};                         // local_a0
        uint32_t range = s_s4_ChildSum ? s_s4_ChildSum(posComp, 0) : 0;
        void* rng = gw ? *reinterpret_cast<void**>(gw + 0xd8) : nullptr;
        uint32_t rnd = (rng && fn_Random_Rand) ? fn_Random_Rand(rng, range) : 0;
        if (s_s4_EntityPos) s_s4_EntityPos(posComp, destPos, 0, rnd, nullptr);

        // build the fetch path (branch 1): dest = source storage, src = trade-post building.
        int64_t destStorage = srcStorPrimary;              // local_d8 = local_b8
        uint64_t citizenVal = static_cast<uint64_t>(*reinterpret_cast<int64_t*>(param2));  // local_b0 = *param2
        uint64_t bldgVal = *reinterpret_cast<uint64_t*>(tradeEntIBP);                       // local_res20 = *local_b0(old)
        uint32_t pathType = *reinterpret_cast<uint32_t*>(selfA + 0x20);
        if (s_s4_FindPath)
            s_s4_FindPath(outBuf, &citizenVal, pathType, &destStorage,
                          reinterpret_cast<float*>(searchPos), &bldgVal,
                          reinterpret_cast<float*>(destPos), static_cast<int64_t>(resType),
                          qty, 1, 0, 1, 0, 1);

        ret = TW_CommitAssign(self, param2, resType, -qty, outBuf);   // NEGATIVE qty (export)
        goto cleanup;
    }

    // ---- 4. PASS 3 over the import list (arrA) -------------------------------
    for (uint32_t ai2 = 0; ai2 < cntA; ai2++) {
        uint64_t resType = arrA[ai2].key;
        void* matDesc = TW_ScanDescComp(resType, rawMatTid);          // pvVar24
        int qty = arrA[ai2].qty;                                      // local_res20 low = pSVar33+8

        // fetch destination cell from the citizen AI sub-object (citizen+0x278 -> +0x68):
        int64_t c2 = *reinterpret_cast<int64_t*>(param2);
        int64_t ai = *reinterpret_cast<int64_t*>(c2 + 0x278);
        int64_t nav = *reinterpret_cast<int64_t*>(ai + 0x68);
        int mapSize = gw ? **reinterpret_cast<int**>(gw + 0xf8) : 0;
        int cellX = TW_ClampCell(*reinterpret_cast<float*>(nav + 0x158), mapSize);
        int cellY = TW_ClampCell(*reinterpret_cast<float*>(nav + 0x15c), mapSize);
        int cell = mapSize * cellY + cellX;

        uint64_t srcPos[2] = {0};                          // local_a0 (out from FindStorage)
        uint64_t srcIBP[2] = {0};                          // local_90 (out IBP from FindStorage)
        int64_t srcStorage = 0;                            // local_e8 (primary result)
        if (s_s4_FindStorage)
            s_s4_FindStorage(reinterpret_cast<void*>(resList), &srcStorage, cell, resType, 1,
                             0, srcPos, srcIBP);
        if (srcStorage == 0) continue;

        // source storage component: scan the FindStorage primary entity (srcStorage/local_e8,
        // entity layout +0x30 array / +0x38 count) for StorageComponent, then its item pairs.
        void* srcStorComp = TW_ScanEntityComp(srcStorage, storageTid);
        int64_t a8 = reinterpret_cast<int64_t>(srcStorComp);
        int qtyMove = qty;
        if (a8 && *reinterpret_cast<int*>(a8 + 0x30) > 0) {
            int64_t pairs = *reinterpret_cast<int64_t*>(a8 + 0x28);
            int pn = *reinterpret_cast<int*>(a8 + 0x30);
            int matchK = -1;
            for (int64_t k = 0; k < pn; k++) {
                if (*reinterpret_cast<uint64_t*>(pairs + k * 0x10) == resType) { matchK = static_cast<int>(k); break; }
            }
            if (matchK < 0) continue;
            int avail = TW_PairAvail(pairs, matchK);
            if (avail == 0) continue;
            if (avail < qtyMove) qtyMove = avail;
        } else {
            continue;
        }

        // clamp by the destination (citizen building) storage free space for resType:
        int64_t citizen = *reinterpret_cast<int64_t*>(param2);
        int64_t destEnt = *reinterpret_cast<int64_t*>(citizen + 0x10);
        void* destStorComp = TW_ScanEntityComp(destEnt, storageTid);  // local_b8
        int64_t b8 = reinterpret_cast<int64_t>(destStorComp);
        if (matDesc && *reinterpret_cast<int*>(reinterpret_cast<int64_t>(matDesc) + 0x48) != 0) {
            int cap  = *reinterpret_cast<int*>(b8 + 0x38);
            int cur  = s_s4_QueryCount ? s_s4_QueryCount(destStorComp) : 0;
            int room = (cap - cur) / *reinterpret_cast<int*>(reinterpret_cast<int64_t>(matDesc) + 0x48);
            if (room < qtyMove) qtyMove = room;
        }

        // gate: can the trade-post building storage accept this resource+qty?
        if (!s_s4_CanAccept || s_s4_CanAccept(bldgStor, resType, qtyMove) == 0)
            continue;

        // source position from the trade post position comp (+ interact-desc base value):
        int64_t interactDesc = *reinterpret_cast<int64_t*>(posComp + 0x18);
        if (!(fn_IsDerivedFrom && interactTid && fn_IsDerivedFrom(reinterpret_cast<void*>(interactDesc), interactTid)))
            interactDesc = 0;
        uint32_t range = interactDesc ? *reinterpret_cast<uint32_t*>(interactDesc + 0x30) : 0;
        if (*reinterpret_cast<int*>(posComp + 0x30) > 0) {
            int64_t children = *reinterpret_cast<int64_t*>(posComp + 0x28);
            int cn = *reinterpret_cast<int*>(posComp + 0x30);
            for (int64_t k = 0; k < cn; k++) {
                int64_t child = *reinterpret_cast<int64_t*>(children + k * 8);
                range += s_s4_ChildSum ? s_s4_ChildSum(child, 0) : 0;
            }
        }
        void* rng = gw ? *reinterpret_cast<void**>(gw + 0xd8) : nullptr;
        uint32_t rnd = (rng && fn_Random_Rand) ? fn_Random_Rand(rng, range) : 0;
        uint64_t srcPos2[2] = {0};                         // local_68
        if (s_s4_EntityPos) s_s4_EntityPos(posComp, srcPos2, 0, rnd, nullptr);

        // build the fetch path (branch 2): dest = trade-post building, src = source storage.
        uint64_t citizenVal = static_cast<uint64_t>(*reinterpret_cast<int64_t*>(param2));   // local_b0 = *param2
        uint64_t bldgVal = *reinterpret_cast<uint64_t*>(tradeEntIBP);                        // local_d8 = *local_b0(old)
        uint64_t srcStorVal = static_cast<uint64_t>(srcStorage);                             // local_90 = local_e8
        uint32_t pathType = *reinterpret_cast<uint32_t*>(selfA + 0x20);
        if (s_s4_FindPath)
            s_s4_FindPath(outBuf, &citizenVal, pathType, &bldgVal,
                          reinterpret_cast<float*>(srcPos2), &srcStorVal,
                          reinterpret_cast<float*>(srcPos), static_cast<int64_t>(resType),
                          qtyMove, 0, 0, 0, 0, 1);

        ret = TW_CommitAssign(self, param2, resType, qtyMove, outBuf);  // POSITIVE qty (import)
        goto cleanup;
    }

cleanup:
    if (s_fn_MemFree) { s_fn_MemFree(arrB); s_fn_MemFree(arrA); }
    return ret;
}


// ============================================================
// TradeWork::Slot6 -- BSTFindRemove + return false (0x1400f5d20)
// ============================================================
bool TradeWork_Slot6_Impl(TradeWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return false;
}


// ============================================================
// TradeWork::Slot7 -- BSTFindRemove thunk (0x1400f5d10)
// ============================================================
int TradeWork_Slot7_Impl(TradeWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return 0;
}


// ============================================================
// TradeWork::Slot8 / CancelAll (0x1400f6be0)
// Loops through BST at +0x48 while count > 0.
// Extended BST nodes: left child at +0x20 (not +0x10).
// ============================================================
int TradeWork_Slot8_Impl(TradeWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    while (*(int*)(selfAddr + 0x50) > 0) {
        // Find leftmost node in BST (following +0x20 = left child for extended nodes)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x20);
        }

        // Cancel the work item
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// TradeWork::Slot9 / RemoveInvalid (0x1400f5ae0)
// Iterates extended BST, validates each entry, removes invalid ones.
// Extended BST: +0x18=parent, +0x20=left, +0x28=right
// Returns true if all valid, false otherwise.
// ============================================================
bool TradeWork_Slot9_Impl(TradeWork* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    void** bstRootPtr = (void**)(selfAddr + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;
    }

    // Find leftmost node (minimum) -- extended BST uses +0x20 for left child
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x20) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x20);
    }

    // In-order traversal of extended BST
    while (node != nullptr) {
        uintptr_t entityIBP = *(uintptr_t*)node;

        // Validate this entity
        bool stillValid = s_fn_ValidateEntity(selfAddr, param2, (void*)&entityIBP, nullptr);

        if (!stillValid) {
            // Add to removal list -- grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                uintptr_t* newArr = removeArr;
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    newArr = (uintptr_t*)s_fn_MemAlloc((int64_t)newCap * 8, 8);
                }

                // Copy existing entries if buffer changed
                uint32_t copyStart = 0;
                if (removeArr == newArr) {
                    copyStart = removeCount;
                }
                for (uint32_t i = copyStart; i < newCount; i++) {
                    uintptr_t val;
                    if (i < removeCount) {
                        val = removeArr[i];
                    } else {
                        val = entityIBP;
                    }
                    newArr[i] = val;
                }
                if (removeArr != newArr) {
                    fn_MemFreeStatic(removeArr);
                }
                removeArr = newArr;
                removeCount = newCount;
            }
        }

        // In-order successor: right child's leftmost descendant, or parent
        // Extended BST: +0x28=right, +0x20=left, +0x18=parent
        void* rightChild = *(void**)((uintptr_t)node + 0x28);
        if (rightChild != nullptr) {
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x20) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x20);
            }
        } else {
            void* parent = *(void**)((uintptr_t)node + 0x18);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x20) != node) {
                while (parent != nullptr && *(void**)((uintptr_t)parent + 0x28) == node) {
                    node = parent;
                    parent = *(void**)((uintptr_t)parent + 0x18);
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove all invalid entries from BST
    // Extended BST: left at +0x20, right at +0x28
    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x20);
                } else {
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        s_fn_BSTRemove(bstRootPtr, searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x28);
                }
            }
        }
    }

    fn_MemFreeStatic(removeArr);
    return removeCount == 0;
}


// ============================================================
// TradeWork::Slot10 / GetWorkEntity (0x1400f6c50)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* TradeWork_Slot10_Impl(TradeWork* self, void* outPtr) {
    // Resolve here as well as in Slot4. A TradeWork does NOT always arrive through
    // TradeWork_Factory / TradeWork_CreateInPlace: TradeComponent_Slot14 (the panel's
    // work checkbox) hand-rolls the allocation — MemAlloc + WorkCtor25 + a manual
    // vtable store — so the factory path, which is the only thing that calls
    // ResolveFuncs, is skipped entirely. The scheduler then calls GetTarget from
    // WorkScheduler_Insert BEFORE Slot4 ever runs, so every s_fn_* here was still its
    // static nullptr and s_fn_FindSubEntity below faulted as EXEC-of-0.
    //
    // (The crash dumper reported that as "VCALL-of-0 ... vtable=ComponentEntity", which
    // is its documented guess: it takes RCX as `this` and prints its first qword. RCX
    // was the entity argument, not a receiver, and the null slots in that vtable were
    // unrelated. The registers were the giveaway — RCX=entity, RDX=&subIBP, matching
    // this call's signature exactly.)
    TradeWork_ResolveFuncs();

    uintptr_t selfAddr = (uintptr_t)self;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// TradeWork::Slot11 / HasWorkers (0x140029860)
// Returns (workerCount > 0). Shared with many work classes.
// ============================================================
bool TradeWork_Slot11_Impl(TradeWork* self) {
    return self->workerCount > 0;
}


// ============================================================
// TradeWork::Execute (Slot5) -- 3 bytes, returns false.
// ============================================================
bool TradeWork_Execute_Impl(TradeWork* /*self*/) {
    return false;
}


// ============================================================
// TradeWork::Serialize (0x1400f6cb0)
// Work::Serialize + Serializer(v0) + BST serialize.
// No extra int field (unlike TendArea).
// ============================================================
void TradeWork_Serialize_Impl(TradeWork* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create serializer (version block, version=0)
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize extended BST at +0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf);

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// TradeWork::DtorBody
// Sets vtable, frees extended BST nodes, calls base dtor.
// ============================================================
void TradeWork_DtorBody(TradeWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to TradeWork
    *(void**)selfAddr = s_vtable_TradeWork;

    // Free BST nodes at +0x48 (extended: left at +0x20, right at +0x28)
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x28));
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(bstRoot);
        }
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set vtable to Work base
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// TradeWork::ScalarDtor (0x14002c660)
// ============================================================
void* TradeWork_ScalarDtor_Impl(TradeWork* self, uint32_t flags) {
    TradeWork_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}


// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161a10 (13 slots), size 0x58, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
// Slot map ground-truthed from the real exe vtable @ 0x140161a10:
//   slot4 (Execute)=TradeWork_Slot4_Impl (0x1400f5d40, STUB). The orphaned
//   TradeWork_Execute_Impl ("returns false") is NOT
//   in the vtable — slot5 is the shared Work_Slot5. Do NOT use it.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TradeWork_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TradeWork");
    return c;
}

static void* g_TradeWork_Vtable[13] = {
    (void*)&TradeWork_ScalarDtor_Impl,  // +0x00 ScalarDtor (0x14002c660)
    (void*)&Work_Slot1,                 // +0x08 slot1 (null)
    (void*)&TradeWork_GetTypeId,        // +0x10 GetTypeId
    (void*)&TradeWork_Serialize_Impl,   // +0x18 Serialize (0x1400f6cb0)
    (void*)&TradeWork_Slot4_Impl,       // +0x20 Execute (0x1400f5d40, STUB)
    (void*)&Work_Slot5,                 // +0x28 slot5 (returns 0)
    (void*)&TradeWork_Slot6_Impl,       // +0x30 OnComplete (0x1400f5d20)
    (void*)&TradeWork_Slot7_Impl,       // +0x38 OnCancel (0x1400f5d10)
    (void*)&TradeWork_Slot8_Impl,       // +0x40 Cleanup (0x1400f6be0)
    (void*)&TradeWork_Slot9_Impl,       // +0x48 Validate (0x1400f5ae0)
    (void*)&TradeWork_Slot10_Impl,      // +0x50 GetTarget (0x1400f6c50)
    (void*)&TradeWork_Slot11_Impl,      // +0x58 GetPriority (0x140029860)
    (void*)&Work_Slot12,                // +0x60 slot12 (void nop)
};

extern "C" void RegisterTradeWorkVtable() {
    RegisterExeAddr(0x140161a10, (uintptr_t)g_TradeWork_Vtable);
}
extern "C" void* TradeWork_CreateInPlace(void* mem) {
    TradeWork_ResolveFuncs();
    return Work_GenericCtor(mem, 0x58, g_TradeWork_Vtable);
}
extern "C" void* TradeWork_Factory(void) {
    TradeWork_ResolveFuncs();
    void* p = Work_Alloc(0x58);
    return p ? Work_GenericCtor(p, 0x58, g_TradeWork_Vtable) : nullptr;
}
REGISTER_TYPE(TradeWork, "Work", &TradeWork_Factory, &TradeWork_CreateInPlace);
