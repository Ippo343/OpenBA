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

#include "Work/TradeWork.h"
#include "Work/tradework_funcs.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Tutorial/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Player/exe_bodies.h"      // B3R: bodies called by name
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <new>
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 D5
#include "Component/StorageComponent.h"         // ENDGAME B.3.9 D5
#include "Component/InteractDescription.h"      // ENDGAME B.3.9 D5

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x14002c8a0  TradeWork_Serialize_c8a0
//   0x14002c690  PlayerTrade_BSTFindRemove_2c690
//   0x140025740  PlayerTrade_BSTRemove_25740
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x140014b00  TradeWork_helper_14b00
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//
// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: TradeWork_Ctor
// disassembles to `0x1400f6dbd: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.


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

// ---- Slot4 callees, called by name (B3R): 0x140015140 / 0x140006270 / 0x140002ed0
// component finders, 0x14012e5c0 CalcMaxWorkers, 0x14012cf10 QueryCount, 0x14012cd80
// CanAccept, 0x14008d590 PickBestDropTarget, 0x140122e20 EntityResolvePos,
// 0x140123070 RecursiveChildSum, 0x1400bf790 FindPath, 0x140115e60 LongTermPlan.

// The already-ported AVL insert (code/Game/Work/tradework_serialize.cpp), registered at
// 0x14002c710. Called with (tree=&self+0x48, out, record{key,extptr,int}).
extern "C" void* TradeWork_Insert_c710(int64_t* tree, void** out, int64_t rec);

namespace {


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
        uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.x bridge (slot 2)
        if (t == tid)
            return (Object::IsDerivedFromNullSafe(c, tid)) ? c : nullptr;
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
        uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.x bridge (slot 2)
        if (t == tid) {
            return (Object::IsDerivedFromNullSafe(c, tid)) ? c : nullptr;
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
            System::MemoryAllocator::MemAlloc(static_cast<int64_t>(static_cast<int>(newCap)) << 4, 8));
    }
    uint32_t start = (oldArr == dst) ? oldCount : 0;
    if (dst) {
        for (uint32_t i = start; i < newCount; i++)
            dst[i] = (i < oldCount) ? oldArr[i] : TWEntry{key, qty, 0};
    }
    if (oldArr != dst && oldArr) System::MemoryAllocator::MemFree(oldArr);
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

    void* r = self->GetTarget(outBuf);   // slot 10, by name (B.3.x)
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

int TradeWork::Execute(void* citizenIBP, void* /*context*/) {

    int64_t selfA = reinterpret_cast<int64_t>(this);
    void* param2 = citizenIBP;                       // local_res10 (citizen IBP)
    uint64_t gw = GameUpdateLoop::_singleton ? reinterpret_cast<uint64_t>(GameUpdateLoop::_singleton) : 0;

    int ret = 1;
    TWEntry* arrA = nullptr; uint32_t cntA = 0, capA = 0;   // import list (net > 0)  -> local_c8/local_c0
    TWEntry* arrB = nullptr; uint32_t cntB = 0, capB = 0;   // export list (net < 0)  -> local_f8/local_f0
    uint64_t outBuf[8] = {0};                               // FindPath / Slot10 scratch (&local_res8)

    // ---- 0. capacity gate ----------------------------------------------------
    int workerCount = static_cast<int>(*reinterpret_cast<int64_t*>(selfA + 0x50));  // param_1[10]
    int64_t workComp = *reinterpret_cast<int64_t*>(selfA + 0x18);                    // param_1[3]
    if (*reinterpret_cast<int*>(workComp + 0x30) == 0)
        *reinterpret_cast<int*>(workComp + 0x30) = WorkComponent_CalcMaxWorkers(workComp);
    if (*reinterpret_cast<int*>(workComp + 0x30) <= workerCount)
        return 1;

    // ---- find components -----------------------------------------------------
    int64_t tradeEntIBP = selfA + 0x10;                   // param_1+2 (&trade-post entity IBP)
    uint64_t tradeCompIBP[2] = {0};                       // local_58 (TradeComponent)
    WorkBase_FindComponent_15140(*reinterpret_cast<uintptr_t*>(tradeEntIBP), tradeCompIBP);
    uint64_t bldgStorIBP[2] = {0};                        // local_50 (building storage)
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(tradeEntIBP), bldgStorIBP);
    uint64_t posCompIBP[2] = {0};                         // local_e0 (position)
    WorkBase_FindComponent_2ed0(*reinterpret_cast<uintptr_t*>(tradeEntIBP), posCompIBP);

    int64_t bldgStor  = static_cast<int64_t>(bldgStorIBP[0]);   // local_78
    int64_t tradeComp = static_cast<int64_t>(tradeCompIBP[0]);
    int64_t posComp   = static_cast<int64_t>(posCompIBP[0]);    // local_e0 value
    uint64_t resList  = gw ? *reinterpret_cast<uint64_t*>(gw + 0xf0) : 0;   // pplVar30 / local_70

    // ---- 1. citizen already carrying trade goods? ---------------------------
    int64_t citizenEnt = *reinterpret_cast<int64_t*>(param2);
    uint64_t carryStorIBP[2] = {0};                       // local_res20
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(citizenEnt + 0x10), carryStorIBP);
    if (CitizenComp_StorageComponent_QueryCount(static_cast<int64_t>(carryStorIBP[0])) > 0) {
        CitizenAI_State_LongTermPlan(static_cast<int64_t>(citizenEnt));
        return 2;
    }

    uint32_t rawMatTid   = RawMaterialDescription::TypeId();
    uint32_t storageTid  = StorageComponent::TypeId();
    uint32_t interactTid = InteractDescription::TypeId();

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
            int cur = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(carryStor));
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
        Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(resList), &srcStorPrimary, cell, resType, qty,
                             1, reinterpret_cast<uint32_t*>(searchPos), srcStorIBP);
        if (srcStorPrimary == 0) { continue; }

        // destination position from the trade post position comp:
        uint64_t destPos[2] = {0};                         // local_a0
        uint32_t range = Pf_RecursiveChildSum_123070(posComp, 0);
        void* rng = gw ? *reinterpret_cast<void**>(gw + 0xd8) : nullptr;
        uint32_t rnd = rng ? reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(range)) : 0;   // B.3.4b bridge
        Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(posComp), reinterpret_cast<float*>(destPos), 0, rnd, nullptr);

        // build the fetch path (branch 1): dest = source storage, src = trade-post building.
        int64_t destStorage = srcStorPrimary;              // local_d8 = local_b8
        uint64_t citizenVal = static_cast<uint64_t>(*reinterpret_cast<int64_t*>(param2));  // local_b0 = *param2
        uint64_t bldgVal = *reinterpret_cast<uint64_t*>(tradeEntIBP);                       // local_res20 = *local_b0(old)
        uint32_t pathType = *reinterpret_cast<uint32_t*>(selfA + 0x20);
        WorkAct_GatherRawMatCtor_bf790(outBuf, &citizenVal, pathType, reinterpret_cast<uint64_t*>(&destStorage),
                          reinterpret_cast<float*>(searchPos), reinterpret_cast<void**>(&bldgVal),
                          reinterpret_cast<float*>(destPos), static_cast<int64_t>(resType),
                          qty, 1, 0, 1, 0, 1);

        ret = TW_CommitAssign(this, param2, resType, -qty, outBuf);   // NEGATIVE qty (export)
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
        Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(resList), &srcStorage, cell, resType, 1,
                             0, reinterpret_cast<uint32_t*>(srcPos), srcIBP);
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
            int cur  = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(destStorComp));
            int room = (cap - cur) / *reinterpret_cast<int*>(reinterpret_cast<int64_t>(matDesc) + 0x48);
            if (room < qtyMove) qtyMove = room;
        }

        // gate: can the trade-post building storage accept this resource+qty?
        if (static_cast<char>(Pf_StorageCanAccept_cd80(bldgStor, resType, qtyMove)) == 0)   // the door read AL
            continue;

        // source position from the trade post position comp (+ interact-desc base value):
        int64_t interactDesc = *reinterpret_cast<int64_t*>(posComp + 0x18);
        if (!(interactTid && Object::IsDerivedFromNullSafe(reinterpret_cast<void*>(interactDesc), interactTid)))
            interactDesc = 0;
        uint32_t range = interactDesc ? *reinterpret_cast<uint32_t*>(interactDesc + 0x30) : 0;
        if (*reinterpret_cast<int*>(posComp + 0x30) > 0) {
            int64_t children = *reinterpret_cast<int64_t*>(posComp + 0x28);
            int cn = *reinterpret_cast<int*>(posComp + 0x30);
            for (int64_t k = 0; k < cn; k++) {
                int64_t child = *reinterpret_cast<int64_t*>(children + k * 8);
                range += Pf_RecursiveChildSum_123070(child, 0);
            }
        }
        void* rng = gw ? *reinterpret_cast<void**>(gw + 0xd8) : nullptr;
        uint32_t rnd = rng ? reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(range)) : 0;   // B.3.4b bridge
        uint64_t srcPos2[2] = {0};                         // local_68
        Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(posComp), reinterpret_cast<float*>(srcPos2), 0, rnd, nullptr);

        // build the fetch path (branch 2): dest = trade-post building, src = source storage.
        uint64_t citizenVal = static_cast<uint64_t>(*reinterpret_cast<int64_t*>(param2));   // local_b0 = *param2
        uint64_t bldgVal = *reinterpret_cast<uint64_t*>(tradeEntIBP);                        // local_d8 = *local_b0(old)
        uint64_t srcStorVal = static_cast<uint64_t>(srcStorage);                             // local_90 = local_e8
        uint32_t pathType = *reinterpret_cast<uint32_t*>(selfA + 0x20);
        WorkAct_GatherRawMatCtor_bf790(outBuf, &citizenVal, pathType, &bldgVal,
                          reinterpret_cast<float*>(srcPos2), reinterpret_cast<void**>(&srcStorVal),
                          reinterpret_cast<float*>(srcPos), static_cast<int64_t>(resType),
                          qtyMove, 0, 0, 0, 0, 1);

        ret = TW_CommitAssign(this, param2, resType, qtyMove, outBuf);  // POSITIVE qty (import)
        goto cleanup;
    }

cleanup:
    System::MemoryAllocator::MemFree(arrB); System::MemoryAllocator::MemFree(arrA);
    return ret;
}


// ============================================================
// TradeWork::Slot6 -- BSTFindRemove + return false (0x1400f5d20)
// ============================================================
int TradeWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    PlayerTrade_BSTFindRemove_2c690(reinterpret_cast<int64_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return false;
}


// ============================================================
// TradeWork::Slot7 -- BSTFindRemove thunk (0x1400f5d10)
// ============================================================
int TradeWork::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    PlayerTrade_BSTFindRemove_2c690(reinterpret_cast<int64_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return 0;
}


// ============================================================
// TradeWork::Slot8 / CancelAll (0x1400f6be0)
// Loops through BST at +0x48 while count > 0.
// Extended BST nodes: left child at +0x20 (not +0x10).
// ============================================================
int TradeWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;

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

        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// TradeWork::Slot9 / RemoveInvalid (0x1400f5ae0)
// Iterates extended BST, validates each entry, removes invalid ones.
// Extended BST: +0x18=parent, +0x20=left, +0x28=right
// Returns true if all valid, false otherwise.
// ============================================================
int TradeWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
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
        bool stillValid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), nullptr, (void*)&entityIBP, nullptr);

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
                    newArr = (uintptr_t*)System::MemoryAllocator::MemAlloc((int64_t)newCap * 8, 8);
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
                    System::MemoryAllocator::MemFree(removeArr);
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
                        PlayerTrade_BSTRemove_25740(reinterpret_cast<int64_t*>(bstRootPtr), searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x28);
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(removeArr);
    return removeCount == 0;
}


// ============================================================
// TradeWork::Slot10 / GetWorkEntity (0x1400f6c50)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* TradeWork::GetTarget(void* outPtr) {

    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    // 0x140002e10 returns its out IBP (RDX) in RAX; the ported body is declared void,
    // so the returned pointer is the out buffer itself.
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entityRef), subIBP);
    void* result = subIBP;


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// TradeWork::GetPriority [11] / HasWorkers (0x140029860)
// Returns (workerCount > 0). Shared with many work classes.
// ============================================================
int TradeWork::GetPriority() {
    // Retail 0x140029860 is `CMP [RCX+0x50],EAX; SETG` — a SIGNED compare.
    return static_cast<int32_t>(workerCount) > 0 ? 1 : 0;
}





// ============================================================
// TradeWork::Serialize (0x1400f6cb0)
// Work::Serialize + Serializer(v0) + BST serialize.
// No extra int field (unlike TendArea).
// ============================================================
void TradeWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create serializer (version block, version=0)
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize extended BST at +0x48
    TradeWork_Serialize_c8a0(reinterpret_cast<int64_t*>((void*)(selfAddr + 0x48)), serializerBuf);

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// TradeWork::DtorBody
// Sets vtable, frees extended BST nodes, calls base dtor.
// ============================================================
// TradeWork dtor (called by slot 0 0x14002c660). The stamps and the `~Internal` call are the compiler's chain.
TradeWork::~TradeWork() {
    uintptr_t selfAddr = (uintptr_t)this;

    // Free BST nodes at +0x48 (extended: left at +0x20, right at +0x28)
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        TradeWork_helper_14b00(reinterpret_cast<int64_t>((void*)(selfAddr + 0x48)), *(void**)((uintptr_t)bstRoot + 0x20));
        TradeWork_helper_14b00(reinterpret_cast<int64_t>((void*)(selfAddr + 0x48)), *(void**)((uintptr_t)bstRoot + 0x28));
        System::MemoryAllocator::MemFree(bstRoot);
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}




// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's.
TradeWork::TradeWork() : Work(), bstRoot(nullptr), workerCount(0), _pad54{} {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_TradeWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over TradeWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus two vtable-pointer statics for the dtor's stamps.

// create 0x14002c620: MemAlloc(sizeof, 8), then the ctor.
void* TradeWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TradeWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TradeWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* TradeWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TradeWork());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(TradeWork, "Work")
