// ============================================================================
// src/engine/collection/set_avl_pathid.h — Set<Pair<Filename, uint32>>
//   The compiled-path -> packfile-id snapshot's element, node and header, and
//   its two entry points (set_avl_pathid.cpp). One declaration: the insert /
//   free bodies and their one caller (io/modification_setupfilesystem.cpp,
//   whose Set is a stack local) walk the same node (B.4 wave X).
// ============================================================================

#pragma once

#include <cstddef>
#include <cstdint>

#include "set_avl.h"   // rtsetavl::Node: the layout the shared retrace serves

namespace rtpathid {

// The element: a refcounted `System::Filename` (one char*) and a uint32.
// 0x1800278c0 writes exactly these two slots and nothing between them, and
// `Set::Node`'s tail starts at +0x10, so the element occupies +0x00..+0x0f
// with +0x0c never written by anybody.
struct PathId {
    char*    key;     // +0x00 — System::Filename::data
    uint32_t value;   // +0x08 — the packfile id
    uint32_t _pad0c;  // +0x0c — NEVER WRITTEN by the ctor or the wrapper
};
static_assert(sizeof(PathId) == 0x10, "the element fills Set::Node's +0x00..+0x0f");
static_assert(offsetof(PathId, value) == 0x08,
              "0x1800278e9 MOV dword ptr [RAX+0x8],ECX");

// B.4 (R-COLL): the node 0x1800278c0 allocates (0x30, align 8) and builds —
// the element above at +0x00, parent +0x10, the two null children +0x18 /
// +0x20 and ONE dword zero at +0x28 over balance and height. It is
// rtsetavl::Node's layout (set_avl.h) with the element typed, so the shared
// retrace 0x1800279f0 serves it. ModificationManager::SetupFileSystem's
// inlined descent reads the same node: 0x1800aef12 `CMP [RBX+0x8],EAX` (the
// value), 0x1800aee9c `MOV RBX,[RBX+0x18]`, 0x1800aeeeb `MOV RBX,[RBX+0x20]`.
struct PathIdNode {
    PathId      element;   // +0x00
    PathIdNode* parent;    // +0x10
    PathIdNode* left;      // +0x18
    PathIdNode* right;     // +0x20
    int16_t     balance;   // +0x28
    int16_t     height;    // +0x2a
    uint32_t    pad2c;     // +0x2c  never written by the ctor
};
static_assert(sizeof(PathIdNode) == 0x30, "0x1800278c0 allocates 0x30");
static_assert(offsetof(PathIdNode, element) == 0x00, "PathIdNode::element @ +0x00");
static_assert(offsetof(PathIdNode, element) + offsetof(PathId, value) == 0x08, "0x1800aef12 CMP [RBX+0x8],EAX");
static_assert(offsetof(PathIdNode, parent)  == offsetof(rtsetavl::Node, parent),  "PathIdNode::parent @ +0x10");
static_assert(offsetof(PathIdNode, left)    == offsetof(rtsetavl::Node, left),    "PathIdNode::left @ +0x18");
static_assert(offsetof(PathIdNode, right)   == offsetof(rtsetavl::Node, right),   "PathIdNode::right @ +0x20");
static_assert(offsetof(PathIdNode, balance) == offsetof(rtsetavl::Node, balance), "PathIdNode::balance @ +0x28");
static_assert(offsetof(PathIdNode, height)  == offsetof(rtsetavl::Node, height),  "PathIdNode::height @ +0x2a");
static_assert(offsetof(PathIdNode, pad2c)   == 0x2c, "PathIdNode::pad2c @ +0x2c");

// The {root, count} header the insert core takes: set_avl.h's `Set` with the
// typed node. SetupFileSystem holds it as a stack local ([RBP-0x49] root,
// [RBP-0x41] count).
struct PathIdSet {
    PathIdNode* root;    // +0x00
    int32_t     count;   // +0x08
    int32_t     pad0c;   // +0x0c
};
static_assert(sizeof(PathIdSet) == sizeof(rtsetavl::Set), "PathIdSet is {root, count}");
static_assert(offsetof(PathIdSet, count) == offsetof(rtsetavl::Set, count), "PathIdSet::count @ +0x08");

PathIdNode** Insert(PathIdSet* set, PathIdNode** outSlot, char* const* key,
                    const uint32_t* value);                     // 0x180027380
void SubtreeFree(void* ignoredThis, PathIdNode* node);          // 0x180027970

}  // namespace rtpathid
