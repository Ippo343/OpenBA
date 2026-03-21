#pragma once
// PerformWork — Generic work-performing action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: PerformWork_Ctor (0x1400c96b0), PerformWork_Factory (0x140022b90)
// Inherits: Object::Internal -> Action (0x20) -> PerformWork
//
// Constructor sequence:
//   Object::Internal::Internal() -> set Action vtable -> init Action fields ->
//   set PerformWork vtable (0x14015fea0) -> init PerformWork fields

#include <cstdint>
#include <cstddef>

namespace PerformWorkAddr {
    constexpr uintptr_t Ctor         = 0x1400c96b0;
    constexpr uintptr_t Factory      = 0x140022b90;
    constexpr uintptr_t Vtable       = 0x14015fea0;
    constexpr uintptr_t RegisterType = 0x14014d290;
}

struct PerformWork {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fea0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer (InternalBasePtr)
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- PerformWork fields ---
    void*    workTarget;         // +0x20: InternalBasePtr — work target entity
    float    facingYaw;          // +0x28: facing yaw angle (degrees, -180..180)
    int32_t  animationId;        // +0x2C: animation ID for SetAnimation
    int64_t  requiredTime;       // +0x30: Time — required work duration
    int64_t  elapsedTime;        // +0x38: Time — elapsed work time
    uint8_t  useTool;            // +0x40: bool — if true, decrement tool durability
    uint8_t  _pad41[3];          // +0x41: padding
    int32_t  field44;            // +0x44: -1 sentinel (unknown purpose)
};

static_assert(sizeof(PerformWork) == 0x48, "PerformWork size mismatch");
static_assert(offsetof(PerformWork, vtable) == 0x00, "");
static_assert(offsetof(PerformWork, workTarget) == 0x20, "");
static_assert(offsetof(PerformWork, facingYaw) == 0x28, "");
static_assert(offsetof(PerformWork, animationId) == 0x2C, "");
static_assert(offsetof(PerformWork, requiredTime) == 0x30, "");
static_assert(offsetof(PerformWork, elapsedTime) == 0x38, "");
static_assert(offsetof(PerformWork, useTool) == 0x40, "");
static_assert(offsetof(PerformWork, field44) == 0x44, "");
