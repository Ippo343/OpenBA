#pragma once
// ============================================================================
// code/Game/Component/AnimalComponent.h — ENDGAME B.3.8 block CIT
//
// `AnimalComponent` (the per-creature component) as a real C++ class with a
// compiler-generated vtable (B38_RULES.md D1-D4). The engine/types POD is a
// forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AnimalComponent", parent "Component", ti 0x1401c6f98
//   create 0x14002f020: MemAlloc(0x58, 8) + ctor 0x140106fc0
//   ctor   0x140106fc0: Component ctor, stamp 0x140162110, +0x28 (dword),
//                       +0x30, +0x38, +0x40 (byte), +0x44 (qword), +0x50 = 0,
//                       flags |= 0xc9
//   dtor   0x14002f170 (scalar, 83 B): restamp, ~Internal, MemFree on &1
//   table  0x140162110, 16 slots:
//     [0] 0x14002f170 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140106e40 Persist                [4] 0x140106d50 Startup (death age)
//     [6] 0x140106c20 OnAdded                [9] 0x140106420 Update
//     [11] 0x140106dd0 Slot11 (104 B)        every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class AnimalComponent final : public Component {
    DECLARE_EXE_TYPE(AnimalComponent)

public:
    AnimalComponent();                                            // 0x140106fc0

    Object::Base* DeletingDtor(unsigned flags) override;          // [0]  0x14002f170
    void Persist(void* stream) override;                          // [3]  0x140106e40
    void Startup() override;                                      // [4]  0x140106d50
    void OnAdded() override;                                      // [6]  0x140106c20
    void Update(void* time) override;                             // [9]  0x140106420
    void Slot11() override;                                       // [11] 0x140106dd0 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1). Names kept
    // from the POD (dataNN) because sibling lanes spell them.
    uint32_t data28;       // +0x28  habitat cell index (y * mapDim + x)
    uint8_t  _pad2C[4];    // +0x2C
    uint64_t data30;       // +0x30  System::Time move timer
    uint64_t data38;       // +0x38  wander box: short x0, y0, width, height
    uint8_t  data40;       // +0x40  non-zero once this animal grazes
    uint8_t  _pad41[3];    // +0x41
    uint32_t data44;       // +0x44  float age, in hours
    uint32_t data48;       // +0x48  float age of death (rolled by Startup)
    uint8_t  _pad4C[4];    // +0x4C
    uint64_t data50;       // +0x50  System::Time graze timer

protected:
    ~AnimalComponent() = default;                                 // no own work
};

static_assert(sizeof(AnimalComponent) == 0x58, "AnimalComponent is 0x58 (create 0x14002f020)");
static_assert(alignof(AnimalComponent) == 8, "AnimalComponent alignment");
static_assert(offsetof(AnimalComponent, data28) == 0x28, "");
static_assert(offsetof(AnimalComponent, data30) == 0x30, "");
static_assert(offsetof(AnimalComponent, data38) == 0x38, "");
static_assert(offsetof(AnimalComponent, data40) == 0x40, "");
static_assert(offsetof(AnimalComponent, data44) == 0x44, "");
static_assert(offsetof(AnimalComponent, data48) == 0x48, "");
static_assert(offsetof(AnimalComponent, data50) == 0x50, "");
