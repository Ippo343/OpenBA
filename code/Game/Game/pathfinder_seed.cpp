// code/Game/Game/pathfinder_seed.cpp
// Phase 3 Keystone D / task #34 — seed the map PATHFINDER worker object at game+0x68.
//
// The pathfinder is a System::Thread subclass (size 0x1f8, vtable 0x14015ef18) that the
// real game allocates during worldgen inside CreateNaturalResource_impl_sub_e8c0
// (0x14009e8c0) and stores at gameObject+0x68. The Game ctor (FUN_14009ee70) only ZEROES
// +0x68, so in standalone it stays null and MoveTo's path setup
// (PathSetup_c8c10 -> SetTerrainCosts_e550 -> Pf_EnqueueCmd1_a6a30) derefs null +0x170.
//
// This function replicates the pathfinder-relevant subset of e8c0 (alloc + the a67f0 ctor +
// the a6110 grid ctor inline + the cost-table / walkability / per-cell-flag priming), then
// stores it at gameObject+0x68. It runs the pathfinder in SYNCHRONOUS mode (+0x5c = 1):
// every enqueue helper (a6a30 / a6c10 / a6b20 / a6430-dispatch) then takes its inline
// `Pf_WorkerApplyCmd_a6430` branch instead of queueing + Thread::Start, so a path query
// completes (runs the A*) on the calling thread before PathfinderQuery returns. No worker
// thread, no vtable virtual ever invoked — the lowest-risk bring-up (Option B from the RE
// spec). The A* closure + enqueue helpers are already reimplemented + registered
// (action_pf_astar.cpp / reg_pf_astar.cpp). Field offsets verified 1:1 vs the Ghidra
// decompiles of FUN_1400a67f0 / FUN_1400a6110 / CreateNaturalResource_impl_sub_e8c0.
//
// Gated behind BANISHED_LIVE_CHARACTER_MOVETICK (the env that enables the move-tick
// executor — the only consumer); default builds stay byte-unchanged.
//
// ENDGAME B.3.8 block LOOP: the object is the REAL class `PathfinderThread`
// (code/Game/Game/PathfinderThread.h); its worker loop is `Run()` and slot 1 is
// System::Thread's OnCompleted, inherited. Gone: the hand-built 2-slot
// `g_pf_vtable` and its no-op OnCompleted stub (the same RET body).

#include "Game/PathfinderThread.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "system/Thread.h"     // ENDGAME B.3.4b System caller batch (CriticalSection/Thread)
#include "Object/exe_type.h"     // DEFINE_EXE_VTABLE

// A* / pathfinder helpers (reimplemented + registered elsewhere) — call directly by name.
extern void Pf_WorkerApplyCmd_a6430(int64_t self, uint32_t* cmd, void* ctxA, void* ctxB);   // 0x1400a6430
extern void Pf_EnqueueCmd1_a6a30(int64_t self, int idx, void* cost, void* ctx);             // 0x1400a6a30
extern void Pf_ArrayResize18_d830(int64_t* arr, int count);                                 // 0x14001d830

namespace {
// Runtime-DLL functions (resolved by mangled name; import strings verified in the exe .rdata).
typedef void* (*fn_ThreadCtor_t)(void* self, unsigned int stackSize);   // ??0Thread@System@@QEAA@I@Z
typedef void* (*fn_CSCtor_t)(void* self);                               // ??0CriticalSection@System@@QEAA@XZ
typedef void  (*pf_fn_MemFree_t)(void* ptr);                            // ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
static fn_ThreadCtor_t s_fn_ThreadCtor = nullptr;
static fn_CSCtor_t     s_fn_CSCtor     = nullptr;
static pf_fn_MemFree_t s_fn_MemFree    = nullptr;

} // namespace

DEFINE_EXE_VTABLE(PathfinderThread, "_ZTV16PathfinderThread")

// --- Pathfinder System::Thread vtable (2 slots) ------------------------------------------
// The pathfinder is a System::Thread subclass; its real vtable (0x14015ef18) is
//   slot 0 = the worker run-loop (FUN_1400a6360),
//   slot 1 = System::Thread::OnCompleted (Runtime 0x18000c590 — a bare RET, no-op).
// System::Thread::Thread(...) already created the OS worker thread (suspended→resumed) inside
// the ctor we call below; System::Thread::ThreadFunc parks it on the "start" event. The
// enqueue helpers (a6a30/a6b20/a6c10) call Thread::Start when +0x5c==0 and the run-flag
// (+0x8 & 1) is clear; Start signals the event, ThreadFunc wakes and invokes slot 0 (drain
// the queue) then slot 1, then clears the run-flag and re-parks. (Verified vs Ghidra: ctor
// 0x1800ca4e0, Start 0x1800ca610, ThreadFunc 0x1800ca440.)
//
// Worker run-loop — faithful 1:1 port of FUN_1400a6360. Pop the head command node under the
// pathfinder CS (+0x30), unlink + free it, then apply via a6430 (runs the A* for cmd type 5
// and publishes the result slot). a6430's param_3/param_4 are dead for the queued path
// (Pf_AstarRun_ce00 `(void)param_4`s the only consumer), so pass null — matching ThreadFunc,
// which invokes slot 0 with `this` only. The command-node MemAlloc (a6c10/a6240) and this
// MemFree, plus the result-array resize, all run under the now-thread-safe global allocator
// lock (standalone/allocator_lock.cpp), so the worker can't race the main thread on the
// engine free-list. Queue: tail@+0x60, head@+0x68, count@+0x70; node qwords [0]=cmd{type,arg1}
// [1]=cmd{arg2,arg3} [2]=next(+0x10) [3]=prev(+0x18).
void PathfinderThread::Run() {
    uint8_t* p   = reinterpret_cast<uint8_t*>(this);   // bridge: the body reads offsets
    int64_t  self = reinterpret_cast<int64_t>(this);
    int count = *reinterpret_cast<int*>(p + 0x70);
    while (count > 0) {
        reinterpret_cast<System::CriticalSection*>(p + 0x30)->Lock();   // B.3.4b bridge
        uint64_t* node  = *reinterpret_cast<uint64_t**>(p + 0x68);    // head
        uint64_t  cmdLo = node[0];                                    // cmd {type, arg1}
        uint64_t  cmdHi = node[1];                                    // cmd {arg2, arg3}
        if (node == *reinterpret_cast<uint64_t**>(p + 0x60))          // head == tail?
            *reinterpret_cast<uint64_t*>(p + 0x60) = node[3];         // tail = node->prev
        *reinterpret_cast<uint64_t*>(p + 0x68) = node[2];            // head = node->next
        if (node[2] != 0) *reinterpret_cast<uint64_t*>(node[2] + 0x18) = node[3];  // next->prev
        if (node[3] != 0) *reinterpret_cast<uint64_t*>(node[3] + 0x10) = node[2];  // prev->next
        node[2] = 0;
        node[3] = 0;
        *reinterpret_cast<int*>(p + 0x70) = *reinterpret_cast<int*>(p + 0x70) - 1;
        if (s_fn_MemFree) s_fn_MemFree(node);
        uint64_t cmd[2] = { cmdLo, cmdHi };
        reinterpret_cast<System::CriticalSection*>(p + 0x30)->Unlock();   // B.3.4b bridge
        Pf_WorkerApplyCmd_a6430(self, reinterpret_cast<uint32_t*>(cmd), nullptr, nullptr);
        count = *reinterpret_cast<int*>(p + 0x70);
    }
}

namespace {
// FUN_1400a6110 — pathfinder cell-grid ctor (grid = pf+0x78). dim = map width.
static void GridCtor(uint8_t* g, int dim) {
    *reinterpret_cast<int*>(g + 0x00)       = 1;
    *reinterpret_cast<uint64_t*>(g + 0x08)  = 0;   // node-array {ptr,count,cap} header
    *reinterpret_cast<uint64_t*>(g + 0x10)  = 0;
    *reinterpret_cast<uint64_t*>(g + 0x18)  = 0;
    *reinterpret_cast<int*>(g + 0x20)       = 0;
    *reinterpret_cast<uint64_t*>(g + 0x28)  = 0;
    *reinterpret_cast<uint64_t*>(g + 0x30)  = 0;
    *reinterpret_cast<uint64_t*>(g + 0x38)  = 8;   // node block size
    *reinterpret_cast<uint64_t*>(g + 0x40)  = 0;
    *reinterpret_cast<int*>(g + 0x48)       = 0;
    Pf_ArrayResize18_d830(reinterpret_cast<int64_t*>(g + 0x08), dim * dim);   // reserve dim*dim A* nodes
    *reinterpret_cast<int*>(g + 0xd0)       = 0;
    *reinterpret_cast<uint16_t*>(g + 0xd4)  = 0;
    *reinterpret_cast<uint8_t*>(g + 0xd6)   = 0;
    void* flags = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton,
                                            static_cast<uint64_t>(dim) * static_cast<uint64_t>(dim), 4) : nullptr;
    *reinterpret_cast<void**>(g + 0xe0)     = flags;   // per-cell terrain-flag byte array
    *reinterpret_cast<int*>(g + 0xd8)       = dim;     // grid width
    for (int i = 0; i < 0x20; ++i)                     // grid cost array (all = 1)
        *reinterpret_cast<int*>(g + 0x50 + i * 4) = 1;
}
} // namespace

// Build + link the pathfinder. gameObj = *(g_pGameWorldSingleton+0xf8); dim = map width.
extern "C" void Pathfinder_Seed(void* gameObjV, int dim) {
    if (!gameObjV || dim <= 0 || !fn_MemAlloc) return;
    uint8_t* game = reinterpret_cast<uint8_t*>(gameObjV);
    if (*reinterpret_cast<void**>(game + 0x68) != nullptr) return;   // already seeded

    if (!s_fn_ThreadCtor) {
        s_fn_ThreadCtor = reinterpret_cast<fn_ThreadCtor_t>(ResolveExport("??0Thread@System@@QEAA@I@Z"));
        s_fn_CSCtor     = reinterpret_cast<fn_CSCtor_t>(ResolveExport("??0CriticalSection@System@@QEAA@XZ"));
        s_fn_MemFree    = reinterpret_cast<pf_fn_MemFree_t>(ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    }
    if (!s_fn_ThreadCtor || !s_fn_CSCtor) {
        fprintf(stderr, "[PfSeed] missing Thread/CS ctor export — skip\n"); fflush(stderr);
        return;
    }

    void* pfV = fn_MemAlloc(g_pMemAllocSingleton, 0x1f8, 4);
    if (!pfV) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(pfV);

    // --- FUN_1400a67f0 ctor (inline) ---
    s_fn_ThreadCtor(pfV, 0x10000);                       // System::Thread::Thread(self, 0x10000) — creates+parks the worker thread
    // Bridge (B4b-2): the Thread ctor door stamped the minted base table; the
    // class's own table goes over it, as retail's ctor stamps 0x14015ef18.
    *reinterpret_cast<const void**>(p)      = PathfinderThread::Vtable();
    s_fn_CSCtor(p + 0x30);                               // System::CriticalSection::CriticalSection(self+0x30)
    *reinterpret_cast<int*>(p + 0x58)       = 1;         // next result-slot generation id
    p[0x5c]                                 = 1;         // priming runs SYNC (a6a30/a6430 apply inline); flipped to 0 (async) after priming
    *reinterpret_cast<uint64_t*>(p + 0x60)  = 0;         // queue tail
    *reinterpret_cast<uint64_t*>(p + 0x68)  = 0;         // queue head
    *reinterpret_cast<int*>(p + 0x70)       = 0;         // queue count
    GridCtor(p + 0x78, dim);                             // FUN_1400a6110
    *reinterpret_cast<uint64_t*>(p + 0x160) = 0;         // result-slot array base
    *reinterpret_cast<uint64_t*>(p + 0x168) = 0;         // result-slot count
    *reinterpret_cast<int*>(p + 0x1f0)      = 0;
    *reinterpret_cast<uint16_t*>(p + 0x1f4) = 0;
    p[0x1f6]                                = 0;
    for (int i = 0; i < 0x20; ++i)                       // staged terrain-cost array (all = 1)
        *reinterpret_cast<int*>(p + 0x170 + i * 4) = 1;

    // Link into the game object.
    *reinterpret_cast<void**>(game + 0x68) = pfV;
    int64_t pf = reinterpret_cast<int64_t>(pfV);

    // --- e8c0 priming (synchronous: every cmd applies inline via a6430) ---
    // cmd type 4 — walkability sub-flags {arg0=1, arg1=0, arg2=0}.
    uint32_t cmd4[4] = { 4, 1, 0, 0 };
    Pf_WorkerApplyCmd_a6430(pf, cmd4, nullptr, nullptr);

    // terrain step-cost table (idx 0..10) — a6a30 applies inline (+0x5c != 0).
    static const int kCosts[11] = { 3, 3, 4, 8, 0x10, 0x10, 0x10, 0x10, 0x10, 3, 4 };
    for (int i = 0; i < 11; ++i)
        Pf_EnqueueCmd1_a6a30(pf, i, reinterpret_cast<void*>(static_cast<intptr_t>(kCosts[i])), nullptr);

    // per-cell terrain flags: low 7 bits = cell terrain type (mapCell+0x29),
    // bit 7 = blocked (mapCell+0x00 first qword != 0). mapCell grid @ game+0x80, stride 0x30.
    uint8_t* flags    = *reinterpret_cast<uint8_t**>(p + 0x158);
    uint8_t* cellGrid = *reinterpret_cast<uint8_t**>(game + 0x80);
    int      cellN    = *reinterpret_cast<int*>(game + 0x88);
    if (flags && cellGrid) {
        for (int i = 0; i < cellN; ++i) {
            uint8_t  type    = cellGrid[i * 0x30 + 0x29];
            bool     blocked = *reinterpret_cast<int64_t*>(cellGrid + i * 0x30) != 0;
            flags[i] = static_cast<uint8_t>((flags[i] & 0x80) | type);
            flags[i] = static_cast<uint8_t>((blocked ? 0x80 : 0) | (flags[i] & 0x7f));
        }
    }

    // Priming is complete. Flip to ASYNC so runtime path queries (cmd 5 via a6c10) queue and
    // wake the worker thread instead of running the A* inline on the main thread. Requires the
    // worker's MemFree to be resolved; otherwise stay sync (safe fallback).
    bool async = s_fn_MemFree != nullptr;
    if (async) p[0x5c] = 0;

    fprintf(stderr, "[PfSeed] pathfinder seeded @ game+0x68 = %p (dim=%d, cells=%d, %s mode)\n",
            pfV, dim, cellN, async ? "ASYNC worker-thread" : "sync");
    fflush(stderr);
}
