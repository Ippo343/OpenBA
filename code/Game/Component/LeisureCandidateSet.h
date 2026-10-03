#pragma once
// ============================================================================
// code/Game/Component/LeisureCandidateSet.h — ENDGAME B.4 P2 block CAI-5
//
// The two candidate sets CitizenAIComponent::State_Leisure (0x14010a560) builds on
// its stack, fills, picks the (Rand % count)-th in-order element of, and frees with
// 0x140014c30 + MemFree. Retail inlines Collection::Set's node code for both; each
// node is MemAlloc(0x28, 8), and both end their inserts in the 0x28-node retrace
// 0x14001a8c0. Neither is a new record: each IS an existing node type, named here.
//
// The home-box set (the reachable entities around the citizen, then its home):
//   0x14010a9d8 / 0x14010aa75 / 0x14010aaaf  MemAlloc(0x28, 8)
//   0x14010a9ea  MOV [RAX],0 ; MOV [RCX],key   +0x00 the element, an
//                                               InternalPtr<ComponentEntity> (qword)
//   0x14010aa92  MOV [RCX+0x8],parent           +0x08 parent
//   0x14010aa96  MOV [RCX+0x10],0               +0x10 left  (taken when key <  node: JC)
//   0x14010aa9a  MOV [RCX+0x18],0               +0x18 right
//   0x14010aa9e  MOV dword [RCX+0x20],0         +0x20 balance + height (one dword)
//   compare: CMP qword (unsigned, JC / JNC) — the key is the entity's address.
//   The other inserts go through 0x140021a30 (the same node).
//   = Collection::Set<ComponentEntity*>::Node (Collection.h; Work.h's
//     WorkCitizenSetNode is the same template over CitizenComponent*).
//
// The cell set (the walkable cells of the box around the target):
//   0x14010af95  MemAlloc(0x28, 8)
//   0x14010afa6  MOV dword [RAX],cell           +0x00 int32 key (cell index)
//   0x14010afaa  MOV [RAX+0x8],0                +0x08 parent
//   0x14010afae  MOV [RAX+0x10],0               +0x10 left  (taken when cell < key: JL, signed)
//   0x14010afb2  MOV [RAX+0x18],0               +0x18 right
//   0x14010afb6  MOV dword [RAX+0x20],0         +0x20 balance + height (one dword)
//   = AvlNode28 (Action/PathAvl.h), whose retrace AvlRebalance<AvlNode28> IS 0x14001a8c0.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "collection/Collection.h"   // Collection::Set<T>::Node
#include "Action/PathAvl.h"          // AvlNode28

class ComponentEntity;

using LeisureHomeNode = Collection::Set<ComponentEntity*>::Node;
static_assert(sizeof(LeisureHomeNode) == 0x28, "MemAlloc(0x28, 8) at 0x14010a9d8");
static_assert(offsetof(LeisureHomeNode, element) == 0x00, "LeisureHomeNode::element @ +0x00 (0x14010a9f1)");
static_assert(offsetof(LeisureHomeNode, parent) == 0x08, "LeisureHomeNode::parent @ +0x08 (0x14010aa92)");
static_assert(offsetof(LeisureHomeNode, left) == 0x10, "LeisureHomeNode::left @ +0x10 (0x14010aa5b)");
static_assert(offsetof(LeisureHomeNode, right) == 0x18, "LeisureHomeNode::right @ +0x18 (0x14010aa49)");
static_assert(offsetof(LeisureHomeNode, balance) == 0x20, "LeisureHomeNode::balance @ +0x20 (0x14010aa9e)");
static_assert(offsetof(LeisureHomeNode, height) == 0x22, "LeisureHomeNode::height @ +0x22 (0x14010aa9e)");

using LeisureCellNode = AvlNode28;
static_assert(sizeof(LeisureCellNode) == 0x28, "MemAlloc(0x28, 8) at 0x14010af95");
static_assert(offsetof(LeisureCellNode, key) == 0x00, "LeisureCellNode::key @ +0x00 (0x14010afa6)");
static_assert(offsetof(LeisureCellNode, parent) == 0x08, "LeisureCellNode::parent @ +0x08 (0x14010afaa)");
static_assert(offsetof(LeisureCellNode, left) == 0x10, "LeisureCellNode::left @ +0x10 (0x14010aff0)");
static_assert(offsetof(LeisureCellNode, right) == 0x18, "LeisureCellNode::right @ +0x18 (0x14010afde)");
static_assert(offsetof(LeisureCellNode, balance) == 0x20, "LeisureCellNode::balance @ +0x20 (0x14010afb6)");
static_assert(offsetof(LeisureCellNode, height) == 0x22, "LeisureCellNode::height @ +0x22 (0x14010afb6)");
