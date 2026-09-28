#pragma once
// ============================================================================
// code/Game/Component/CharacterComponent.h — ENDGAME B.3.8 block CIT
//
// `CharacterComponent` (movement + the action stack of citizens and animals) as
// a real C++ class with a compiler-generated vtable (B38_RULES.md D1-D4). The
// engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CharacterComponent", parent "Component", ti 0x1401c7078
//   create 0x14002f960: MemAlloc(0x80, 8) + ctor 0x140109d20
//   ctor   0x140109d20: Component ctor, stamp 0x140162370, +0x28 (byte) = 0,
//                       +0x2c = +0x34 = 0, +0x40 = -1, action stack at +0x48
//                       {0, 0, 0, top +0x60 = -1}, +0x68 = +0x70 = +0x78 = 0,
//                       flags |= 0x8f
//   dtor   0x14002faa0 (scalar): restamp, 0x1400c5150(+0x48) (the action-stack
//                       release), stamp 0x140162920, ~Internal, MemFree on &1
//   table  0x140162370, 16 slots:
//     [0] 0x14002faa0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140109ab0 Persist                [4] 0x140109a30 Startup
//     [6] 0x1401099c0 OnAdded (char index)   [7] 0x140109980 OnRemove
//     [8] 0x1401094c0 Tick (move-tick, full rate)   [9] 0x140109480 Update
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class CharacterComponent final : public Component {
    DECLARE_EXE_TYPE(CharacterComponent)

public:
    CharacterComponent();                                         // 0x140109d20

    // PORT GAP (retail 0x14002faa0) — B38 D2: retail releases the action stack
    // at +0x48 (0x1400c5150); the tree never did (Inert_ScalarDtor).
    ~CharacterComponent() override = default;                     // [0] 0x14002faa0
    void Persist(void* stream) override;                          // [3] 0x140109ab0
    void Startup() override;                                      // [4] 0x140109a30
    void OnAdded() override;                                      // [6] 0x1401099c0 PORT GAP
    void OnRemove() override;                                     // [7] 0x140109980
    void Tick(void* time) override;                               // [8] 0x1401094c0
    void Update(void* time) override;                             // [9] 0x140109480

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint8_t  data28;       // +0x28
    uint8_t  _pad29[3];    // +0x29
    uint32_t data2C;       // +0x2C
    uint32_t data30;       // +0x30
    uint32_t data34;       // +0x34
    uint32_t data38;       // +0x38
    uint8_t  _pad3C[4];    // +0x3C
    int32_t  data40;       // +0x40 char index (-1 = unregistered)
    uint8_t  _pad44[4];    // +0x44
    uint64_t data48;       // +0x48 action stack data
    uint64_t data50;       // +0x50 action stack count / capacity
    uint64_t data58;       // +0x58
    int32_t  data60;       // +0x60 action stack top (-1 = empty)
    uint8_t  _pad64[4];    // +0x64
    uint64_t data68;       // +0x68 ModelComponent
    uint64_t data70;       // +0x70 PickingComponent
    uint64_t data78;       // +0x78 CharacterDescription
};

static_assert(sizeof(CharacterComponent) == 0x80, "CharacterComponent is 0x80 (create 0x14002f960)");
static_assert(alignof(CharacterComponent) == 8, "CharacterComponent alignment");
static_assert(offsetof(CharacterComponent, data28) == 0x28, "");
static_assert(offsetof(CharacterComponent, data2C) == 0x2C, "");
static_assert(offsetof(CharacterComponent, data30) == 0x30, "");
static_assert(offsetof(CharacterComponent, data34) == 0x34, "");
static_assert(offsetof(CharacterComponent, data38) == 0x38, "");
static_assert(offsetof(CharacterComponent, data40) == 0x40, "");
static_assert(offsetof(CharacterComponent, data48) == 0x48, "");
static_assert(offsetof(CharacterComponent, data50) == 0x50, "");
static_assert(offsetof(CharacterComponent, data58) == 0x58, "");
static_assert(offsetof(CharacterComponent, data60) == 0x60, "");
static_assert(offsetof(CharacterComponent, data68) == 0x68, "");
static_assert(offsetof(CharacterComponent, data70) == 0x70, "");
static_assert(offsetof(CharacterComponent, data78) == 0x78, "");
