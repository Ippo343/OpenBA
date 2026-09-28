#pragma once
// ============================================================================
// code/Game/Component/TornadoComponent.h — ENDGAME B.3.8 block CIT
//
// `TornadoComponent` (the tornado disaster's path, movement and damage) as a real
// C++ class with a compiler-generated vtable (B38_RULES.md D1-D4). The
// engine/types POD is a forwarder to this file. (The visual `Tornado`
// Graphics::Particle class is block LOOP's.)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TornadoComponent", parent "Component", ti 0x1401c4ad8
//   create 0x14000d500: MemAlloc(0x68, 8) + ctor 0x140058cf0
//   ctor   0x140058cf0: Component ctor, stamp 0x14015aa80, +0x28 = 0, path
//                       block at +0x30 {+0x30 = 0, +0x38 (dword) = 0, +0x40 =
//                       +0x48 = 0, +0x50 = -1}, +0x58 = 0, +0x60 (dword) = 0,
//                       flags |= 0x84 (bit 2: the full-rate tree walk -> slot 8)
//   dtor   0x14000d640 (scalar): restamp, MemFree(+0x40), +0x40 = +0x48 = 0
//                       (the waypoint array), stamp 0x140162920, ~Internal,
//                       MemFree on &1
//   table  0x14015aa80, 16 slots:
//     [0] 0x14000d640 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140058b80 Persist                [4] 0x1400585a0 Startup (path)
//     [8] 0x140057cd0 Tick (per frame)       every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class TornadoComponent final : public Component {
    DECLARE_EXE_TYPE(TornadoComponent)

public:
    TornadoComponent();                                           // 0x140058cf0

    // PORT GAP (retail 0x14000d640) — B38 D2: retail frees the waypoint array
    // at +0x40; the tree never did (Inert_ScalarDtor).
    ~TornadoComponent() override = default;                       // [0] 0x14000d640
    void Persist(void* stream) override;                          // [3] 0x140058b80
    void Startup() override;                                      // [4] 0x1400585a0
    void Tick(void* time) override;                               // [8] 0x140057cd0

    // PUBLIC while the ported bodies read them (B38 decision 1).
    int64_t  damageCountdown;   // +0x28 System::Time; reloaded from desc+0x28
    float    waypointX;         // +0x30 current path target (Vector3)
    float    waypointY;         // +0x34
    float    waypointZ;         // +0x38
    uint8_t  _pad3C[4];         // +0x3C
    void*    pathData;          // +0x40 Vector3[] (freed by the retail dtor)
    int32_t  pathCount;         // +0x48
    int32_t  pathCapacity;      // +0x4C
    int32_t  pathIndex;         // +0x50 ctor -1; current waypoint index
    uint8_t  _pad54[4];         // +0x54
    float    posX;              // +0x58 live world position (Vector3)
    float    posY;              // +0x5C
    float    posZ;              // +0x60
    uint8_t  _pad64[4];         // +0x64
};

static_assert(sizeof(TornadoComponent) == 0x68, "TornadoComponent is 0x68 (create 0x14000d500)");
static_assert(alignof(TornadoComponent) == 8, "TornadoComponent alignment");
static_assert(offsetof(TornadoComponent, damageCountdown) == 0x28, "");
static_assert(offsetof(TornadoComponent, waypointX)       == 0x30, "");
static_assert(offsetof(TornadoComponent, pathData)        == 0x40, "");
static_assert(offsetof(TornadoComponent, pathCount)       == 0x48, "");
static_assert(offsetof(TornadoComponent, pathCapacity)    == 0x4C, "");
static_assert(offsetof(TornadoComponent, pathIndex)       == 0x50, "");
static_assert(offsetof(TornadoComponent, posX)            == 0x58, "");
static_assert(offsetof(TornadoComponent, posZ)            == 0x60, "");
