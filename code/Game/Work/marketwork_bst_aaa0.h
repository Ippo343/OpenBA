// code/Game/Work/marketwork_bst_aaa0.h
// Wave (MarketWork BST) -- the ERASE cluster: 0x14002aaa0 -> 0x14002ac40 -> 0x14002aee0.
//
// See marketwork_bst_aaa0.cpp for the full derivation.  Summary of the true roles
// (Ghidra's names and the wave brief both mis-label 0x14002aaa0 as "insert"):
//
//   0x14002aaa0  erase-BY-KEY   : search the tree for a key, call 0x14002ac40 on the hit.
//   0x14002ac40  erase-BY-NODE  : unlink one node, destroy it, AVL-retrace.
//   0x14002aee0  destroy-node   : free the node's payload array, then the node.
//
// Node layout (0x38 bytes):
//   +0x00 key (InternalBasePtr)   +0x08 payload array ptr   +0x10 count:int32/cap:int32
//   +0x18 parent   +0x20 left   +0x28 right   +0x30 balance:int16   +0x32 height:int16
// Tree header (MarketWork+0x48):  +0x00 root (8 bytes)   +0x08 count (int32)

#pragma once

#include <cstdint>

extern "C" {

// 0x14002aaa0 -- __fastcall void(int64_t* tree [RCX], void* keyIBP [RDX])
// Finds the node whose key equals *keyIBP and erases it.  No-op if absent.
void MarketBst_aaa0(int64_t* tree, void* keyIBP);

// 0x14002ac40 -- __fastcall void(int64_t* tree [RCX], void* node [RDX])
// Erases an already-located node: decrements the count, unlinks, frees, rebalances.
void MarketBst_ac40(int64_t* tree, void* node);

// 0x14002aee0 -- __fastcall void(void* unusedRcx [RCX], void* node [RDX])
// Destroys one node.  RCX is a genuinely DEAD parameter in retail (homed, never read);
// it is kept in the signature so the address registry binds an ABI-identical function.
void MarketBst_aee0(void* unusedRcx, void* node);

} // extern "C"
