// code/Game/Action/keystone_a_zone.cpp
// Keystone A (zone + road enqueue cluster) — faithful 1:1 ports of the exe
// ZoneComponent zone-tile rasterizer (0x14009d360), RoadTool zone-cost enqueuer
// (0x14009ce00) + its helper (0x1400a6e30), and the type-3 worker-queue enqueue
// (0x1400a6f00). These feed the pathfinder worker-thread command queue whose core
#include "Action/exe_bodies.h"
// (a6430/a6b20/a6c10/a6d50/a6a30/a68b0) is already built+registered in
// action_pf_astar.cpp / action_move / etc.
//
// NONE of this is exercised on the autostart path, so a wrong transcription is
// undetectable silent corruption — every offset below is cross-checked against the
// Ghidra decompile AND disassembly. Do NOT "improve" or simplify.
//
// Registration point: RegisterKeystoneAZoneBatch() in standalone/reg_keystone_a_zone.cpp.
//
// ============================================================================
// COMMAND-NODE layout (worker-thread queue; identical to action_pf_astar.cpp):
//   +0x00 (int) cmdType   +0x04 (int) arg1   +0x08 (int) arg2   +0x0c (int) arg3
//   +0x10 (ptr) next-link  +0x18 (ptr) prev-link
// Thread/queue struct: +0x08(byte) running-flags, +0x30 CriticalSection,
//   +0x5c(byte) on-worker-thread, +0x60 tail, +0x68 head, +0x70 count.
//
// ZoneComponent (param_1 of 9d360, an int*; BYTE offsets below):
//   +0x00 (int)  grid row stride (map width)              (*param_1)
//   +0x68 (ptr)  worker-thread queue object (Thread*)     (param_1 + 0x1a)
//   +0x80 (ptr)  cell grid base, stride 0x30              (param_1 + 0x20)
// Per-cell (stride 0x30): +0x00 (ptr) zone IBP, +0x28 (byte) zone-ref count.
//
// RoadTool (param_1 of 9ce00): +0x68 (ptr) worker-thread queue object (Thread*).
// ============================================================================

#include <windows.h>
#include <new>
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Thread.h"     // ENDGAME B.3.4b System callers
#include "application/Application.h"   // ENDGAME B.3.7 callers (K2)

// Match the action_pf_*.cpp decompile token aliases.
typedef unsigned int  uint;

namespace {

// ---- Exe sub-callees in this batch's closure, called by name (B3R):
// 0x1400a6a30 Pf_EnqueueCmd1_a6a30, 0x1400a68b0 Pf_EnqueueCmd4_a68b0,
// 0x1400a6b20 Pf_EnqueueCmd0_a6b20, 0x1400a6c10 Pf_EnqueueTradeRoute_a6c10,
// 0x1400a6d50 ActMv_CopyPathFromFinder_a6d50, 0x1400a6430 Pf_WorkerApplyCmd_a6430.

// ---- Shared enqueue tail// ---- Shared enqueue tail: identical to action_pf_astar.cpp's EnqueueNode. ----
static void EnqueueNode(int64_t self, uint32_t* localCmd) {
    reinterpret_cast<System::CriticalSection*>(self + 0x30)->Lock();   // B.3.4b bridge
    uint32_t* node = reinterpret_cast<uint32_t*>(System::MemoryAllocator::MemAlloc(0x20, 8));
    if (node != nullptr) {
        node[0] = localCmd[0];
        node[1] = localCmd[1];
        node[2] = localCmd[2];
        node[3] = localCmd[3];
        *reinterpret_cast<uint64_t*>(node + 4) = 0;  // +0x10 next
        *reinterpret_cast<uint64_t*>(node + 6) = 0;  // +0x18 prev
    }
    if (*reinterpret_cast<int64_t*>(self + 0x60) == 0) {
        *reinterpret_cast<uint32_t**>(self + 0x68) = node;
    } else {
        *reinterpret_cast<uint32_t**>(*reinterpret_cast<int64_t*>(self + 0x60) + 0x10) = node;
        *reinterpret_cast<uint64_t*>(node + 6) = *reinterpret_cast<uint64_t*>(self + 0x60);
    }
    *reinterpret_cast<int*>(self + 0x70) = *reinterpret_cast<int*>(self + 0x70) + 1;
    *reinterpret_cast<uint32_t**>(self + 0x60) = node;
    reinterpret_cast<System::CriticalSection*>(self + 0x30)->Unlock();   // B.3.4b bridge
    if ((*reinterpret_cast<uint8_t*>(self + 8) & 1) == 0) {
        reinterpret_cast<System::Thread*>(self)->Start();   // B.3.4b bridge
    }
}

} // namespace

// ============================================================================
// 0x1400a6e30 — RoadTool worker helper. (Ghidra: RoadTool_helper_6e30.)
//   Enqueues the trade-route request, then spin-waits (yielding to the worker
//   thread) until the requested slot is populated, then copies the path out.
//   Closure: a6b20, a6c10, a6d50 (all reg) + Runtime YieldToThreads/CS/ThreadStart.
// signature mirrors the decompile:
//   (Thread* param_1, uint param_2, ulonglong param_3, void* param_4,
//    longlong param_5, int param_6)
// ============================================================================
// ⚠ RETURNS THE PATH LENGTH. Ghidra types both this and its tail call `void`, but
// 1400a6edf is a tail call into 0x1400a6d50, whose EAX is min(maxLen, cells found) —
// and the boat route builder (boatcomponent_route.cpp) tests exactly that value to
// decide whether a leg found anything. This port was `void` until the merchant-arrival
// layer needed it; the pre-existing caller below discards the value, so widening it
// changes nothing else. See reference_discarded_return_reenables_gated_branch.
int RoadTool_helper_6e30(int64_t param_1, uint param_2, uint64_t param_3,
                         void* param_4, int64_t param_5, int param_6) {
    // The request slot is TWO ADJACENT ints, not two independent locals: a6c10 ->
    // TradeComponent_impl_6240 (0x1400a6240) writes BOTH `*param_2` (slot index, retail
    // [RSP+0x30]) and `param_2[1]` (slot key, [RSP+0x34]) through the single pointer it
    // is handed. Declaring them as separate `int` locals and passing only &local_res8
    // left the key write landing on whatever the compiler happened to place next, so
    // the key compared against uninitialised stack and the wait below NEVER matched —
    // an unbounded spin that hung the whole game on the first road cost query.
    int slot[2] = {0, 0};   // [0] = slot index, [1] = slot key
    int& slotIndex = slot[0];
    int& slotKey   = slot[1];

    // FUN_1400a6b20(param_1, (int)param_4, param_3, param_4)
    Pf_EnqueueCmd0_a6b20(param_1, static_cast<int>(reinterpret_cast<uint64_t>(param_4)),
                         reinterpret_cast<void*>(param_3), param_4);

    // TradeComponent_impl_6c10(param_1, slot, param_2, param_3 & 0xffffffff)
    Pf_EnqueueTradeRoute_a6c10(param_1, slot,
                               reinterpret_cast<void*>(static_cast<uint64_t>(param_2)),
                               reinterpret_cast<void*>(param_3 & 0xffffffff));

    do {
        if (slotIndex < *reinterpret_cast<int*>(param_1 + 0x168)) {
            int64_t base = *reinterpret_cast<int64_t*>(param_1 + 0x160);
            if (*reinterpret_cast<int*>(base + 4 + static_cast<int64_t>(slotIndex) * 0x18) == slotKey &&
                *reinterpret_cast<char*>(base + static_cast<int64_t>(slotIndex) * 0x18) != '\0') {
                // MoveTo_helper_6d50(param_1, slot, param_5, param_6)
                return ActMv_CopyPathFromFinder_a6d50(reinterpret_cast<void*>(param_1), slot,
                                                      reinterpret_cast<void*>(param_5), param_6);
            }
        }
        // Application::Application::YieldToThreads(*g_pAppSingleton)
        ::Application::Application::_singleton->YieldToThreads();   // B.3.7 bridge
        reinterpret_cast<System::CriticalSection*>(param_1 + 0x30)->Lock();   // B.3.4b bridge
        if (*reinterpret_cast<int*>(param_1 + 0x70) > 0 &&
            (*reinterpret_cast<uint8_t*>(param_1 + 8) & 1) == 0) {
            reinterpret_cast<System::Thread*>(param_1)->Start();   // B.3.4b bridge
        }
        reinterpret_cast<System::CriticalSection*>(param_1 + 0x30)->Unlock();   // B.3.4b bridge
    } while (true);
}

// ============================================================================
// 0x1400a6f00 — enqueue cmd type 3 (set zone-grid IBP-present byte). (Ghidra: FUN_1400a6f00.)
//   Same shape as a6fe0 but cmd[0]=3, cmd[2]=(uint)param3.
// signature: (Thread* param_1, uint32 param_2, uint64 param_3, void* param_4)
// ============================================================================
void Pf_EnqueueCmd3_a6f00(int64_t param_1, uint32_t param_2, uint64_t param_3, void* param_4) {
    uint32_t local[4];
    local[0] = 3;
    local[1] = param_2;
    local[2] = static_cast<uint32_t>(param_3);
    local[3] = 0;
    if (*reinterpret_cast<uint8_t*>(param_1 + 0x5c) == 0) {
        EnqueueNode(param_1, local);
    } else {
        // FUN_1400a6430(param_1, &local, param_3, param_4) — apply directly on worker.
        // a6430 lives in action_pf_astar.cpp.
        Pf_WorkerApplyCmd_a6430(param_1, local, reinterpret_cast<void*>(param_3), param_4);
    }
}

// ============================================================================
// 0x14009d360 — ZoneComponent zone-tile rasterizer. (Ghidra: ZoneComponent_sub_d360.)
//   Walks a (param_5 x param_6) tile rectangle at (param_3,param_4), and for each
//   cell either clears the zone IBP (param_2==null sentinel) or — when the
//   rotated source mask char == '.' — installs a fresh InternalBasePtr. Each cell
//   write enqueues a type-3 worker command (Pf_EnqueueCmd3_a6f00).
//   param_8 is the rotation selector (0x5a / 0xb4 / 0x10e).
//   param_1 is int*; BYTE offsets: *param_1 (row stride), +0x80 grid base.
//   Closure: Object::InternalBasePtr ctor + 0x1400a6f00 (this file).
// signature mirrors the decompile:
//   (int* param_1, longlong* param_2, int param_3, int param_4, uint param_5,
//    int param_6, longlong* param_7, int param_8)
// ============================================================================
void ZoneComponent_sub_d360(int* param_1, int64_t* param_2, int param_3, int param_4,
                            uint param_5, int param_6, int64_t* param_7, int param_8) {
    uint64_t uVar7 = static_cast<uint64_t>(static_cast<uint>(param_6));
    uint64_t uVar12 = static_cast<uint64_t>(static_cast<uint>(static_cast<int>(param_5)));
    int iVar17 = 0;
    uint64_t uVar8 = uVar12;
    int iVar6 = param_6;
    if (0 < param_6) {
        do {
            iVar6 = iVar6 + -1;
            int iVar13 = 0;
            int iVar14 = static_cast<int>(uVar8);
            if (0 < iVar14) {
                do {
                    iVar14 = iVar14 + -1;
                    int64_t lVar2 = *reinterpret_cast<int64_t*>(
                        reinterpret_cast<char*>(param_1) + 0x80);  // grid base
                    int iVar15 = (iVar17 + param_4) * *param_1 + iVar13 + param_3;
                    int64_t lVar3 = *param_7;
                    bool bVar18;
                    if ((lVar3 == 0) ||
                        (*reinterpret_cast<int*>(lVar3 + -8) == 0)) {
                        bVar18 = true;
                    } else {
                        bVar18 = false;
                    }
                    bVar18 = !bVar18;
                    int64_t* plVar9;
                    int64_t local_58;
                    int64_t local_50[2];
                    if (bVar18) {
                        int iVar11 = iVar14;
                        int iVar4 = iVar17;
                        uint64_t uVar16;
                        int iVar10;
                        if ((param_8 == 0x5a) ||
                            ((uVar8 = uVar7, uVar16 = uVar12, iVar10 = iVar6,
                              param_8 != 0xb4 &&
                              (iVar10 = iVar17, iVar11 = iVar13, iVar4 = iVar6,
                               param_8 == 0x10e)))) {
                            uVar8 = uVar12;
                            uVar16 = uVar7;
                            iVar10 = iVar11;
                            iVar11 = iVar4;
                        }
                        bVar18 = *reinterpret_cast<char*>(
                                     static_cast<int64_t>(iVar11) +
                                     (uVar16 + 1) * ((uVar8 - static_cast<int64_t>(iVar10)) - 1) +
                                     lVar3) == '.';
                        if (!bVar18) goto LAB_clear;
                        new (&local_58) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                        plVar9 = &local_58;
                    } else {
LAB_clear:
                        local_50[0] = *param_2;
                        plVar9 = local_50;
                    }
                    lVar3 = *plVar9;
                    *reinterpret_cast<int64_t*>(
                        *reinterpret_cast<int64_t*>(reinterpret_cast<char*>(param_1) + 0x80) +
                        static_cast<int64_t>(iVar15) * 0x30) = lVar3;
                    // FUN_1400a6f00(*(Thread**)(param_1+0x68), iVar15, (lVar3 != 0), param_1)
                    Pf_EnqueueCmd3_a6f00(
                        *reinterpret_cast<int64_t*>(reinterpret_cast<char*>(param_1) + 0x68),
                        static_cast<uint32_t>(iVar15),
                        static_cast<uint64_t>(lVar3 != 0),
                        param_1);
                    if (bVar18) {
                        char cVar5 = -1;
                        if (*param_2 != 0) cVar5 = '\x01';
                        char* pcVar1 = reinterpret_cast<char*>(
                            lVar2 + 0x28 + static_cast<int64_t>(iVar15) * 0x30);
                        *pcVar1 = static_cast<char>(*pcVar1 + cVar5);
                    }
                    uVar8 = static_cast<uint64_t>(param_5);
                    iVar13 = iVar13 + 1;
                } while (iVar13 < static_cast<int>(param_5));
            }
            iVar17 = iVar17 + 1;
        } while (iVar17 < param_6);
    }
}

// ============================================================================
// 0x14009ce00 — RoadTool zone-cost enqueuer. (Ghidra: RoadTool_sub_ce00.)
//   Pushes 11 per-index cost commands (a6a30 idx 0..10, value 1) + two flag
//   commands (a68b0) onto the worker queue, then tail-calls the helper to run
//   the trade-route A* and copy the resulting path.
//   param_1 (RoadTool) +0x68 = worker-thread queue object (Thread*).
//   Closure: a6a30, a68b0 (reg) + a6e30 (this file).
// signature mirrors the decompile:
//   (longlong param_1, uint param_2, uint param_3, ulonglong param_4,
//    undefined8 param_5, longlong param_6, byte param_7, byte param_8)
// RETURNS THE PATH LENGTH (B3R Leftovers, read in retail): the body ends in a tail
// JMP 0x1400a6e30 at 0x14009cf41 after restoring RBX/RBP/RSI/RDI/R14, so EAX at its
// return is 0x1400a6e30's EAX -- set by its CALL 0x1400a6d50 at 0x1400a6edf and not
// touched before the RET at 0x1400a6ef8 -- i.e. RoadTool_helper_6e30's int. Its
// readers (Player/roadtool.cpp, roadtool_calcpath.cpp) use it as the path length.
// param_5 is never read (retail overwrites its stack slot at 0x14009cf22 with
// param_6 for the tail call).
// ============================================================================
int RoadTool_sub_ce00(int64_t param_1, uint param_2, uint param_3, uint64_t param_4,
                       void* param_5, int64_t param_6, uint8_t param_7, uint8_t param_8) {
    int64_t pTVar1 = *reinterpret_cast<int64_t*>(param_1 + 0x68);
    uint64_t uVar2 = param_4;
    void* p4 = reinterpret_cast<void*>(param_4);
    void* p4b = reinterpret_cast<void*>(uVar2);
    {
        Pf_EnqueueCmd1_a6a30(pTVar1, 0, reinterpret_cast<void*>(1), p4);
        Pf_EnqueueCmd1_a6a30(pTVar1, 1, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 2, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 3, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 4, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 5, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 6, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 7, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 8, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 9, reinterpret_cast<void*>(1), p4b);
        Pf_EnqueueCmd1_a6a30(pTVar1, 10, reinterpret_cast<void*>(1), p4b);
    }
    // RoadTool_sub_ce00_sub_68b0(self, 0, 0, 0)
    Pf_EnqueueCmd4_a68b0(*reinterpret_cast<int64_t*>(param_1 + 0x68), 0, nullptr, nullptr);
    // RoadTool_sub_ce00_sub_68b0(self, 0, param_7, param_8)
    Pf_EnqueueCmd4_a68b0(*reinterpret_cast<int64_t*>(param_1 + 0x68), 0,
                         reinterpret_cast<void*>(static_cast<uint64_t>(param_7)),
                         reinterpret_cast<void*>(static_cast<uint64_t>(param_8)));
    // RoadTool_helper_6e30(self, param_3, param_4 & 0xffffffff, param_2, param_6, 0x800)
    return RoadTool_helper_6e30(*reinterpret_cast<int64_t*>(param_1 + 0x68),
                         param_3, param_4 & 0xffffffff,
                         reinterpret_cast<void*>(static_cast<uint64_t>(param_2)),
                         param_6, 0x800);
}
