#pragma once
// ============================================================================
// code/Game/Game/PathfinderThread.h — ENDGAME B.3.8 block LOOP (B38_RULES sec. 5,
// "the 5 unreflected exe subclasses of Runtime classes")
//
// `PathfinderThread` (0x1f8): the map pathfinder at Game+0x68, the exe's
// unreflected `System::Thread` subclass. Its worker drains the queued path
// commands (A*). Retail has no name for it; the name is the tree's.
//
// RETAIL (Application-x64.exe): table 0x14015ef18, 2 slots, MemAlloc(0x1f8),
// ctor 0x1400a67f0 (inlined in CreateNaturalResource_impl_sub_e8c0), dtor
// 0x1400a6720.
//   [0] 0x1400a6360 Run          (the worker loop)
//   [1] 0x18000c590 OnCompleted  (System::Thread's, a bare RET — inherited)
//
// CONSTRUCTION (ENDGAME B.3.9 ENG W2): `::new (p) PathfinderThread(0x10000)`
// on System::Thread's real ctor (pathfinder_seed.cpp). Field layout from the
// ctor is documented there; the members read offsets.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/Thread.h"

class PathfinderThread final : public System::Thread {
public:
    void Run() override;                                  // [0] 0x1400a6360  WORKER THREAD
    // [1] OnCompleted: inherited (System::Thread's RET)

    static const void* Vtable();

    // Retail's ctor 0x1400a67f0 (inlined in CreateNaturalResource_impl_sub_e8c0):
    // `??0Thread@System@@QEAA@I@Z` with 0x10000, then the 0x14015ef18 stamp (the
    // compiler's). ENDGAME B.3.9 ENG W2; the field stores stay in pathfinder_seed.cpp.
    explicit PathfinderThread(unsigned int stackSize) : System::Thread(stackSize) {}

protected:
    ~PathfinderThread() = default;                        // torn down by GameDtor_ThreadDtor_a6720

public:
    // +0x30 CriticalSection, +0x58 result-slot generation, +0x5c sync flag,
    // +0x60/+0x68/+0x70 command queue (tail, head, count), +0x78 cell grid
    // (0xe8 bytes), +0x160 result slots, +0x170 staged cost table (0x20 ints),
    // +0x1f0.. flags.
    uint8_t _fields[0x1f8 - 0x30];                        // +0x30
};

static_assert(sizeof(PathfinderThread) == 0x1f8, "PathfinderThread is MemAlloc(0x1f8)");
static_assert(offsetof(PathfinderThread, _fields) == 0x30, "fields follow the 0x30 Thread base");
