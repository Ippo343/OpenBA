#pragma once
// ============================================================================
// code/Game/Dialog/ToolRegistryNode.h — B4 P2 DLG-2
//
// One node of the tool registry at PlayerToolManager +0x10 (root) / +0x18
// (count): an AVL Set keyed by the type id of a sub-description a tool-group
// template carries, holding the Tool class to build and the ToolSetting the
// first activation creates. No retail symbol; the tree's name.
//
// RETAIL (Application-x64.exe):
//   alloc    0x14002df10 (called by the find-or-create insert 0x14002ddb0):
//            MemAlloc(0x38, 8); dword +0x00 = key, dword +0x08 = the Tool's type
//            id, qword +0x10 = the ToolSetting (0, then copied), qword +0x18 =
//            the parent, qwords +0x20 / +0x28 = 0, dword +0x30 = 0
//   retrace  0x140014b40 (shared by the 0x38-byte Sets): word +0x32 the height
//            (`MOVZX R8D, word [RAX+0x32]`), word +0x30 the balance
//            (`MOV word [RDX+0x30], CX` = right height - left height)
//   free     0x14002de90 (root; zeroes root and count) / 0x14002df90 (subtree):
//            left +0x20, right +0x28, then `DeletePtr(+0x10)` when non-null,
//            then MemFree
//   walk     PlayerToolManager::SelectTool 0x140102b60 (Dialog/toolbardialog.cpp)
// The +0x04 / +0x0C dwords are never written by the allocator.
// ============================================================================

#include <cstdint>
#include <cstddef>

class ToolSetting;   // Player/ToolSetting.h

struct ToolRegistryNode {
    uint32_t          typeId;          // +0x00 the key: a sub-description's type id
    uint32_t          _pad04;          // +0x04
    uint32_t          factoryTypeId;   // +0x08 the Tool class Factory::CreateObject builds
    uint32_t          _pad0C;          // +0x0C
    ToolSetting*      toolSetting;     // +0x10 InternalPtr<ToolSetting>, created lazily
    ToolRegistryNode* parent;          // +0x18
    ToolRegistryNode* left;            // +0x20
    ToolRegistryNode* right;           // +0x28
    int16_t           balance;         // +0x30 right height - left height (0x140014b40)
    int16_t           height;          // +0x32 (0x140014b40)
    uint32_t          _pad34;          // +0x34
};
static_assert(sizeof(ToolRegistryNode) == 0x38, "the tool registry node is MemAlloc(0x38, 8) (0x14002df10)");
static_assert(offsetof(ToolRegistryNode, typeId) == 0x00, "ToolRegistryNode +0x00");
static_assert(offsetof(ToolRegistryNode, _pad04) == 0x04, "ToolRegistryNode +0x04");
static_assert(offsetof(ToolRegistryNode, factoryTypeId) == 0x08, "ToolRegistryNode +0x08");
static_assert(offsetof(ToolRegistryNode, _pad0C) == 0x0C, "ToolRegistryNode +0x0C");
static_assert(offsetof(ToolRegistryNode, toolSetting) == 0x10, "ToolRegistryNode +0x10");
static_assert(offsetof(ToolRegistryNode, parent) == 0x18, "ToolRegistryNode +0x18");
static_assert(offsetof(ToolRegistryNode, left) == 0x20, "ToolRegistryNode +0x20");
static_assert(offsetof(ToolRegistryNode, right) == 0x28, "ToolRegistryNode +0x28");
static_assert(offsetof(ToolRegistryNode, balance) == 0x30, "ToolRegistryNode +0x30");
static_assert(offsetof(ToolRegistryNode, height) == 0x32, "ToolRegistryNode +0x32");
static_assert(offsetof(ToolRegistryNode, _pad34) == 0x34, "ToolRegistryNode +0x34");
