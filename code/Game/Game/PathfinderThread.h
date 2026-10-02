#pragma once
// ============================================================================
// code/Game/Game/PathfinderThread.h — ENDGAME B.3.8 block LOOP (B38_RULES sec. 5,
// "the 5 unreflected exe subclasses of Runtime classes"); fields split by
// ENDGAME B.4 W1 ACT-a (the header's owner from B.4 W1 on).
//
// `PathfinderThread` (0x1f8): the map pathfinder at Map+0x68, the exe's
// unreflected `System::Thread` subclass. Its worker drains the queued path
// commands (A*). Retail has no name for it; the name is the tree's, and so are
// the field names (descriptive, unconfirmed; the record structs are in
// code/Game/Action/PathSearch.h).
//
// RETAIL (Application-x64.exe): table 0x14015ef18, 2 slots, MemAlloc(0x1f8),
// ctor 0x1400a67f0 (inlined in CreateNaturalResource_impl_sub_e8c0), dtor
// 0x1400a6720.
//   [0] 0x1400a6360 Run          (the worker loop)
//   [1] 0x18000c590 OnCompleted  (System::Thread's, a bare RET — inherited)
//
// CONSTRUCTION (ENDGAME B.3.9 ENG W2): `::new (p) PathfinderThread(0x10000)`
// on System::Thread's real ctor (pathfinder_seed.cpp), which then writes every
// field below by hand, as retail's ctor does. EVERY FIELD IS TRIVIALLY
// CONSTRUCTIBLE AND DESTRUCTIBLE (asserted at the bottom): the class's own ctor
// must construct nothing and its dtor (keystone_a_gamedtor.cpp) frees the fields
// by hand, so a member ctor / dtor would run twice. `lock` is the real
// `System::CriticalSection`, which has neither (its init / teardown are the
// explicit `??0` / `??1` calls the ctor and dtor make).
//
// FIELDS (the ctor 0x1400a67f0 and the command dispatcher 0x1400a6430):
//   +0x30 lock           System::CriticalSection (Lock / Unlock around the queue
//                        and the result slots)
//   +0x58 nextToken      result-slot token counter, never 0 (0x1400a6240)
//   +0x5c runInline      nonzero: an enqueuer applies its command at once
//                        instead of queueing it (0x1400a6fe0 `CMP [RCX+0x5c],DIL`)
//   +0x60/+0x68/+0x70    command queue tail / head / count (0x1400a6360 pops
//                        the head)
//   +0x78 grid           the worker's grid search, 0xE8 (PathSearch.h)
//   +0x160 results       Collection::Array<PathResult>, as its three raw fields
//   +0x170 stagedStepCost[0x20], +0x1f0 stagedPassableTypes, +0x1f4..+0x1f6
//                        stagedDiagonal / stagedManhattan / stagedRespectBlocked:
//                        the main thread's copy of what it last sent (the
//                        enqueuers compare against these and send only changes)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include "system/Thread.h"
#include "Action/PathSearch.h"

class PathfinderThread final : public System::Thread {
public:
    void Run() override;                                  // [0] 0x1400a6360  WORKER THREAD
    // [1] OnCompleted: inherited (System::Thread's RET)


    // Retail's ctor 0x1400a67f0 (inlined in CreateNaturalResource_impl_sub_e8c0):
    // `??0Thread@System@@QEAA@I@Z` with 0x10000, then the 0x14015ef18 stamp (the
    // compiler's). ENDGAME B.3.9 ENG W2; the field stores stay in pathfinder_seed.cpp.
    explicit PathfinderThread(unsigned int stackSize) : System::Thread(stackSize) {}

    // Retail's dtor 0x1400a6720 (keystone_a_gamedtor.cpp): the field teardown,
    // then `??1CriticalSection` on +0x30, then System::Thread's dtor (the
    // compiler's chain; it was the `??1Thread` door until ENDGAME B.3.9 VS).
    // PUBLIC and non-virtual: retail's Game::~Game (0x14009ed60) calls it by
    // name on the Game+0x68 pointer, then MemFrees it.
    ~PathfinderThread();
    // 0x1400a6690 (GAME-b, keystone_a_gamedtor.cpp): start the worker while work is
    // queued, yield until the queue is empty, then WaitForFinish.
    void DrainQueue();

    // ---- the command API (B.4 ACT-a; bodies in code/Game/Action/action_pf_astar.cpp
    // unless noted). Each enqueuer compares against its staged copy, stores the new
    // value, and either applies the command at once (runInline) or queues it and starts
    // the worker. Arguments are retail's registers.
    void ApplyCommand(const PathCommandArgs& cmd);          // 0x1400a6430 (RCX, RDX only)
    void SetPassableTypes(int mask);                        // 0x1400a6b20 command 0
    void SetStepCost(int terrainType, int cost);            // 0x1400a6a30 command 1
    void SetCellTerrain(int cell, int terrainType);         // 0x1400a6fe0 command 2
    void SetSearchFlags(uint8_t diagonal, uint8_t manhattan,
                        uint8_t respectBlocked);            // 0x1400a68b0 command 4
    PathTicket* RequestPath(PathTicket* ticket, int start, int goal);  // 0x1400a6c10 command 5
    PathTicket* AllocResult(PathTicket* ticket);            // 0x1400a6240
    int  TakePath(PathTicket* ticket, int32_t* path, int maxNodes);    // 0x1400a6d50 (action_base_move.cpp)

    // ---- fields -----------------------------------------------------------
    System::CriticalSection lock;          // +0x30
    int32_t      nextToken;                // +0x58
    uint8_t      runInline;                // +0x5C
    uint8_t      _pad5D[3];                // +0x5D
    PathCommand* queueTail;                // +0x60
    PathCommand* queueHead;                // +0x68
    int32_t      queueCount;               // +0x70
    uint8_t      _pad74[4];                // +0x74
    GridPathSearch grid;                   // +0x78
    RawArray<PathResult> results;          // +0x160 Collection::Array<PathResult>, raw (the dtor frees it)
    int32_t      stagedStepCost[0x20];     // +0x170
    int32_t      stagedPassableTypes;      // +0x1F0
    uint8_t      stagedDiagonal;           // +0x1F4
    uint8_t      stagedManhattan;          // +0x1F5
    uint8_t      stagedRespectBlocked;     // +0x1F6
    uint8_t      _pad1F7;                  // +0x1F7
};

static_assert(sizeof(PathfinderThread) == 0x1f8, "PathfinderThread is MemAlloc(0x1f8)");
static_assert(offsetof(PathfinderThread, lock) == 0x30, "PathfinderThread::lock @ +0x30");
static_assert(offsetof(PathfinderThread, nextToken) == 0x58, "PathfinderThread::nextToken @ +0x58");
static_assert(offsetof(PathfinderThread, runInline) == 0x5C, "PathfinderThread::runInline @ +0x5C");
static_assert(offsetof(PathfinderThread, queueTail) == 0x60, "PathfinderThread::queueTail @ +0x60");
static_assert(offsetof(PathfinderThread, queueHead) == 0x68, "PathfinderThread::queueHead @ +0x68");
static_assert(offsetof(PathfinderThread, queueCount) == 0x70, "PathfinderThread::queueCount @ +0x70");
static_assert(offsetof(PathfinderThread, grid) == 0x78, "PathfinderThread::grid @ +0x78");
static_assert(offsetof(PathfinderThread, results) == 0x160, "PathfinderThread::results @ +0x160");
static_assert(offsetof(PathfinderThread, stagedStepCost) == 0x170, "PathfinderThread::stagedStepCost @ +0x170");
static_assert(offsetof(PathfinderThread, stagedPassableTypes) == 0x1F0, "PathfinderThread::stagedPassableTypes @ +0x1F0");
static_assert(offsetof(PathfinderThread, stagedDiagonal) == 0x1F4, "PathfinderThread::stagedDiagonal @ +0x1F4");
static_assert(offsetof(PathfinderThread, stagedManhattan) == 0x1F5, "PathfinderThread::stagedManhattan @ +0x1F5");
static_assert(offsetof(PathfinderThread, stagedRespectBlocked) == 0x1F6, "PathfinderThread::stagedRespectBlocked @ +0x1F6");
// H5 (the coordinator's condition 1): declaring the fields must start no ctor and
// no dtor. Every field type is trivially constructible and destructible.
static_assert(std::is_trivially_default_constructible<System::CriticalSection>::value &&
              std::is_trivially_destructible<System::CriticalSection>::value,
              "PathfinderThread::lock: no member ctor / dtor may run");
static_assert(std::is_trivially_default_constructible<GridPathSearch>::value &&
              std::is_trivially_destructible<GridPathSearch>::value,
              "PathfinderThread::grid: no member ctor / dtor may run");
