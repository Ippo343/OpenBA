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
// stay byte arrays that the tree's ctor builds with placement new (and whose
// MeshInstance the dtor destroys by an explicit call). B.4 H5: declaring them
// with their real types would make the compiler construct them a second time
// (and destroy the MeshInstance twice); that is a `behaviour:` commit of its
// own (LISTED by W0-H2).
//
// B.4 W0-H2 fields (retail Persist 0x140124b70, Startup 0x140124ac0, ctor):
//   +0x164 int16 meshGroup  Serialize(short&); index into ModelDescription's
//                           mesh-group array (+0x20, stride 0x18); Startup copies
//                           ModelDescription+0x30 (a Rand if +0x38 is set)
//   +0x166 int16 meshIndex  Serialize(short&); index into that group's mesh
//                           array (group+8, stride 8); < 0 = no mesh (OnAdded /
//                           OnRemove / Persist test it)
//   +0x168 float yaw        Serialize(float&); the placement yaw (degrees) the
//                           +0x148 transform was built from; CharacterComponent
//                           copies it into its facing
//   ctor: one qword 0 over +0x164..+0x16B, +0x16C = -1
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
    // Re-point every mesh material to one material set (defined in
    // destroycomponent_state6.cpp, B4 COMP-c).
    void SetMaterialSet(int materialSetIdx);                      // 0x140125020

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint8_t  meshInstance[0x120]; // +0x28 Graphics::MeshInstance (door-built)
    uint8_t  transform[0x1C];     // +0x148 Math::Transform
    int16_t  meshGroup;           // +0x164 mesh-group index
    int16_t  meshIndex;           // +0x166 mesh index in the group (< 0 = none)
    float    yaw;                 // +0x168 placement yaw, degrees
    int32_t  modelId;             // +0x16C scene id (ctor -1 = not in scene)
};

static_assert(sizeof(ModelComponent) == 0x170, "ModelComponent is 0x170 (create 0x1400358d0)");
static_assert(alignof(ModelComponent) == 8, "ModelComponent alignment");
static_assert(offsetof(ModelComponent, meshInstance) == 0x28, "");
static_assert(offsetof(ModelComponent, transform) == 0x148, "");
static_assert(offsetof(ModelComponent, meshGroup) == 0x164, "ModelComponent::meshGroup @ +0x164 (Serialize short&)");
static_assert(offsetof(ModelComponent, meshIndex) == 0x166, "ModelComponent::meshIndex @ +0x166 (Serialize short&)");
static_assert(offsetof(ModelComponent, yaw) == 0x168, "ModelComponent::yaw @ +0x168 (Serialize float&)");
static_assert(offsetof(ModelComponent, modelId) == 0x16C, "");
