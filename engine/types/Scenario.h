#pragma once
// Scenario — Game scenario (extends Achievement), binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x90.
//
// Decompiled from: Scenario_Factory (0x14001e9f0)
// Inherits: Achievement::Achievement -> Scenario
// Scenario extends Achievement base (0x58 bytes), adding scenario-specific fields.

#include <cstdint>
#include <cstddef>

namespace ScenarioAddr {
    constexpr uintptr_t Factory      = 0x14001e9f0;
    constexpr uintptr_t Vtable       = 0x14015f020;
    constexpr uintptr_t RegisterType = 0x14014cd10;
    constexpr uintptr_t Cleanup      = 0x14001ea70;
}

struct Scenario {
    // --- Achievement base (0x58 bytes, constructed by Achievement::Achievement) ---
    void*    vtable;             // +0x00: -> 0x14015f020
    uint8_t  _achievBase[0x50];  // +0x08: Achievement base fields

    // --- Scenario fields ---
    void*    _field58;           // +0x58: zeroed (scenario map?)
    void*    _field60;           // +0x60: zeroed
    uint16_t _field68;           // +0x68: 1 (enabled flags?)
    uint8_t  _pad6A[6];          // +0x6A: padding
    void*    _field70;           // +0x70: zeroed
    void*    _field78;           // +0x78: zeroed
    void*    _field80;           // +0x80: zeroed
    void*    _field88;           // +0x88: zeroed
};

static_assert(sizeof(Scenario) == 0x90, "Scenario size mismatch");
static_assert(offsetof(Scenario, vtable) == 0x00, "");
