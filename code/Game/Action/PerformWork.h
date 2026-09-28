#pragma once
// ============================================================================
// code/Game/Action/PerformWork.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `PerformWork`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PerformWork", parent "Action", ti 0x1401c5fd8
//   create 0x140022b90: MemAlloc(0x48, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x14015fea0, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class PerformWork final : public Action {
    DECLARE_EXE_TYPE(PerformWork)

public:
    PerformWork();                                                   // create 0x140022b90
    PerformWork(void* entity, float facingYaw, uint32_t animationId, int64_t time, uint8_t useTool,
                int field44);   // 0x1400c9720
    ~PerformWork() override = default;                         // [0] 0x140022bd0
    void Persist(void* stream) override;                       // [3] 0x1400c9550
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c9420
    int  Execute(void* actionStack, void* time) override;      // [5] 0x1400c93c0
    void Exit(void* actionStack) override;                     // [7] 0x1400c9370
    void CheckEntity() override;                               // [8] 0x1400c9320
    bool GetTarget(void* param) override;                      // [9] 0x1400c92d0

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    void*     workTarget;        // +0x20 InternalBasePtr: work target entity
    float     facingYaw;         // +0x28 facing yaw (degrees)
    int32_t   animationId;       // +0x2C animation id
    int64_t   requiredTime;      // +0x30 Time: required work duration
    int64_t   elapsedTime;       // +0x38 Time: elapsed work time
    uint8_t   useTool;           // +0x40 bool: decrement tool durability
    uint8_t   _pad41[3];         // +0x41 padding
    int32_t   field44;           // +0x44 -1 sentinel
};

static_assert(sizeof(PerformWork) == 0x48, "PerformWork is 0x48 (create 0x140022b90 MemAlloc)");
static_assert(alignof(PerformWork) == 8, "PerformWork is 8-aligned");
static_assert(offsetof(PerformWork, workTarget) == 0x20, "PerformWork +0x20");
static_assert(offsetof(PerformWork, facingYaw) == 0x28, "PerformWork +0x28");
static_assert(offsetof(PerformWork, animationId) == 0x2c, "PerformWork +0x2c");
static_assert(offsetof(PerformWork, requiredTime) == 0x30, "PerformWork +0x30");
static_assert(offsetof(PerformWork, elapsedTime) == 0x38, "PerformWork +0x38");
static_assert(offsetof(PerformWork, useTool) == 0x40, "PerformWork +0x40");
static_assert(offsetof(PerformWork, field44) == 0x44, "PerformWork +0x44");
