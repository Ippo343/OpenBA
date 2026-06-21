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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// A* / pathfinder helpers (reimplemented + registered elsewhere) — call directly by name.
extern void Pf_WorkerApplyCmd_a6430(int64_t self, uint32_t* cmd, void* ctxA, void* ctxB);   // 0x1400a6430
extern void Pf_EnqueueCmd1_a6a30(int64_t self, int idx, void* cost, void* ctx);             // 0x1400a6a30
extern void Pf_ArrayResize18_d830(int64_t* arr, int count);                                 // 0x14001d830

namespace {
// Runtime-DLL ctors (resolved by mangled name; import strings verified in the exe .rdata).
typedef void* (*fn_ThreadCtor_t)(void* self, unsigned int stackSize);   // ??0Thread@System@@QEAA@I@Z
typedef void* (*fn_CSCtor_t)(void* self);                               // ??0CriticalSection@System@@QEAA@XZ
static fn_ThreadCtor_t s_fn_ThreadCtor = nullptr;
static fn_CSCtor_t     s_fn_CSCtor     = nullptr;

// A never-invoked stub vtable for the pathfinder (size 2: worker-loop slot 0, dtor slot 1).
// In synchronous mode Thread::Start is never called, so slot 0 (the worker loop) never runs;
// the object is never freed, so slot 1 never runs. Present only so *pf is a valid pointer.
static void  PfVtbl_Stub0(void*) {}
static void* PfVtbl_Stub1(void*, unsigned) { return nullptr; }
static void* g_pf_stub_vtable[2] = { reinterpret_cast<void*>(&PfVtbl_Stub0),
                                     reinterpret_cast<void*>(&PfVtbl_Stub1) };

static bool MoveTickEnabled() {
    const char* e = getenv("BANISHED_LIVE_CHARACTER_MOVETICK");
    return e && e[0] && e[0] != '0';
}

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
    if (!MoveTickEnabled()) return;
    if (!gameObjV || dim <= 0 || !fn_MemAlloc) return;
    uint8_t* game = reinterpret_cast<uint8_t*>(gameObjV);
    if (*reinterpret_cast<void**>(game + 0x68) != nullptr) return;   // already seeded

    if (!s_fn_ThreadCtor) {
        s_fn_ThreadCtor = reinterpret_cast<fn_ThreadCtor_t>(ResolveExport("??0Thread@System@@QEAA@I@Z"));
        s_fn_CSCtor     = reinterpret_cast<fn_CSCtor_t>(ResolveExport("??0CriticalSection@System@@QEAA@XZ"));
    }
    if (!s_fn_ThreadCtor || !s_fn_CSCtor) {
        fprintf(stderr, "[PfSeed] missing Thread/CS ctor export — skip\n"); fflush(stderr);
        return;
    }

    void* pfV = fn_MemAlloc(g_pMemAllocSingleton, 0x1f8, 4);
    if (!pfV) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(pfV);

    // --- FUN_1400a67f0 ctor (inline) ---
    s_fn_ThreadCtor(pfV, 0x10000);                       // System::Thread::Thread(self, 0x10000)
    *reinterpret_cast<void**>(p)            = g_pf_stub_vtable;   // real = 0x14015ef18 (worker loop); stub never called in sync mode
    s_fn_CSCtor(p + 0x30);                               // System::CriticalSection::CriticalSection(self+0x30)
    *reinterpret_cast<int*>(p + 0x58)       = 1;         // next result-slot generation id
    p[0x5c]                                 = 1;         // SYNCHRONOUS MODE (real ctor sets 0)
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

    fprintf(stderr, "[PfSeed] pathfinder seeded @ game+0x68 = %p (dim=%d, cells=%d, sync mode)\n",
            pfV, dim, cellN);
    fflush(stderr);
}
