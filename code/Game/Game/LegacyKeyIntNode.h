#pragma once
// ============================================================================
// code/Game/Game/LegacyKeyIntNode.h — B.4 wave X
//
// The 0x30-byte node of the key -> int tree that CitizenList's version < 2 save reads
// (citizenlist_serialize.cpp, SerializeLegacyKeyIntTree). It rebalances through the
// shared 0x30-node walk 0x14000e360 (CitizenAI_Reg_helper_e360, Component/exe_bodies.h),
// which reads int16 balance / height at +0x28 / +0x2A, and the 0x30 rotations
// 0x14000e450 / 0x14000e580 (Action/PathAvl.h): both are instantiated for it.
// ============================================================================

#include <cstdint>
#include <cstddef>
class ComponentDescription;   // Component/ComponentDescription.h

struct LegacyKeyIntNode {
    ComponentDescription* key;     // +0x00 ExternalPtr<ComponentDescription> (0x14000ebe0 serializes it through 0x140002340; typed B8 RT2)
    int32_t           value;       // +0x08 (the high dword is zeroed with it: one qword store)
    uint32_t          valueHigh;   // +0x0C
    LegacyKeyIntNode* parent;      // +0x10
    LegacyKeyIntNode* left;        // +0x18
    LegacyKeyIntNode* right;       // +0x20
    int16_t           balance;     // +0x28 \ zeroed by the ctor as one dword
    int16_t           height;      // +0x2A /
    uint8_t           _pad2C[4];   // +0x2C
};
static_assert(sizeof(LegacyKeyIntNode) == 0x30, "LegacyKeyIntNode is MemAlloc(0x30)");
static_assert(offsetof(LegacyKeyIntNode, key) == 0x0, "LegacyKeyIntNode::key");
static_assert(offsetof(LegacyKeyIntNode, _pad2C) == 0x2C, "LegacyKeyIntNode::_pad2C");
static_assert(offsetof(LegacyKeyIntNode, value) == 0x08, "LegacyKeyIntNode::value @ +0x08");
static_assert(offsetof(LegacyKeyIntNode, valueHigh) == 0x0C, "LegacyKeyIntNode::valueHigh @ +0x0C");
static_assert(offsetof(LegacyKeyIntNode, parent) == 0x10, "LegacyKeyIntNode::parent @ +0x10");
static_assert(offsetof(LegacyKeyIntNode, left) == 0x18, "LegacyKeyIntNode::left @ +0x18");
static_assert(offsetof(LegacyKeyIntNode, right) == 0x20, "LegacyKeyIntNode::right @ +0x20");
static_assert(offsetof(LegacyKeyIntNode, balance) == 0x28, "LegacyKeyIntNode::balance @ +0x28");
static_assert(offsetof(LegacyKeyIntNode, height) == 0x2A, "LegacyKeyIntNode::height @ +0x2A");
