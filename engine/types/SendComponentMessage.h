#pragma once
// SendComponentMessage — Action to send a message to a component, binary-compatible struct.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: SendComponentMessage_Factory (0x140023070, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> SendComponentMessage

#include <cstdint>
#include <cstddef>

namespace SendComponentMessageAddr {
    constexpr uintptr_t Factory = 0x140023070;
    constexpr uintptr_t Vtable  = 0x1401600c0;
}

struct SendComponentMessage {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401600c0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- SendComponentMessage fields ---
    void*    _field20;           // +0x20: zeroed (target component?)
    void*    _field28;           // +0x28: zeroed (message data?)
    uint32_t _field30;           // +0x30: zeroed (message type?)
    int32_t  _field34;           // +0x34: -1 sentinel (message ID?)
    void*    _field38;           // +0x38: zeroed
};

static_assert(sizeof(SendComponentMessage) == 0x40, "SendComponentMessage size mismatch");
static_assert(offsetof(SendComponentMessage, vtable) == 0x00, "");
