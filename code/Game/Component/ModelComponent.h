#pragma once
// ============================================================================
// code/Game/Component/ModelComponent.h — ENDGAME B.3.8 block VIS
//
// `ModelComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ModelComponent", parent "Component", ti 0x1401c7a50
//   create 0x1400358d0: MemAlloc(0x170, 8) + ctor 0x140124d20
//   ctor   0x140124d20: Component ctor, stamp 0x140163ba0,
//                       Graphics::MeshInstance ctor on +0x28 (import slot
//                       0x140151490), Math::Transform ctor on +0x148,
//                       +0x164 = 0 (qword), +0x16c = -1, flags |= 0x83
//   dtor   0x140124cd0: restamp, ~MeshInstance(+0x28), restamp, ~Internal
//   table  0x140163ba0, 16 slots (the tree's old table had 24):
//     [0] 0x140035ad0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140124b70 Persist                [4] 0x140124ac0 Startup (Ghidra "Serialize": set the mesh)
//     [6] 0x140124a30 OnAdded (Ghidra "Draw": scene add)
//     [7] 0x1401249d0 OnRemove (Ghidra "OnCreate")
//     [8] 0x140124960 Tick (Ghidra "OnRemove": mesh Update)
//     every other slot = Component's
//
// The embedded Graphics::MeshInstance (+0x28) and Math::Transform (+0x148)
// stay byte arrays built through the Runtime's MeshInstance ctor DOOR (import
// slot 0x140151490), exactly as the tree's factory built them: converting them
// to real Runtime members (a compiler-stamped MeshInstance vptr instead of the
// door's) is not part of this step.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class ModelComponent final : public Component {
    DECLARE_EXE_TYPE(ModelComponent)

public:
    ModelComponent();                                             // 0x140124d20

    // 0x140124cd0
    ~ModelComponent() override;                                   // [0] 0x140035ad0
    void Persist(void* stream) override;                          // [3] 0x140124b70
    void Startup() override;                                      // [4] 0x140124ac0
    void OnAdded() override;                                      // [6] 0x140124a30
    void OnRemove() override;                                     // [7] 0x1401249d0
    void Tick(void* time) override;                               // [8] 0x140124960

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint8_t  meshInstance[0x120]; // +0x28 Graphics::MeshInstance (door-built)
    uint8_t  transform[0x1C];     // +0x148 Math::Transform
    uint8_t  _pad164[8];          // +0x164 variantIdx / modelVariant / scale (ctor 0)
    int32_t  modelId;             // +0x16C scene id (ctor -1 = not in scene)
};

static_assert(sizeof(ModelComponent) == 0x170, "ModelComponent is 0x170 (create 0x1400358d0)");
static_assert(alignof(ModelComponent) == 8, "ModelComponent alignment");
static_assert(offsetof(ModelComponent, meshInstance) == 0x28, "");
static_assert(offsetof(ModelComponent, transform) == 0x148, "");
static_assert(offsetof(ModelComponent, _pad164) == 0x164, "");
static_assert(offsetof(ModelComponent, modelId) == 0x16C, "");
