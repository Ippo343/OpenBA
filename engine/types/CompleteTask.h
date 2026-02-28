#pragma once
// CompleteTask — Action to complete a work task, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: CompleteTask_Factory (0x140021fc0, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> CompleteTask

#include <cstdint>
#include <cstddef>

namespace CompleteTaskAddr {
    constexpr uintptr_t Factory = 0x140021fc0;
    constexpr uintptr_t Vtable  = 0x14015fbd0;
}

struct CompleteTask {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fbd0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- CompleteTask fields ---
    void*    workEntity;         // +0x20: work entity ref (DAT_1401c7158 type)
    void*    targetEntity;       // +0x28: target entity ref (ComponentEntity type)
    int32_t  taskIndex;          // +0x30: task index (0x7FFFFFFF = unresolved)
    int32_t  taskParam;          // +0x34: task parameter
};

static_assert(sizeof(CompleteTask) == 0x38, "CompleteTask size mismatch");
static_assert(offsetof(CompleteTask, vtable) == 0x00, "");
static_assert(offsetof(CompleteTask, workEntity) == 0x20, "");
static_assert(offsetof(CompleteTask, targetEntity) == 0x28, "");
static_assert(offsetof(CompleteTask, taskIndex) == 0x30, "");
static_assert(offsetof(CompleteTask, taskParam) == 0x34, "");
