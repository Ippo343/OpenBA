// code/Game/Work/marketwork_bst_b290.h
// MarketWork market-BST SERIALIZE pair — 0x14002b290 (tree) + 0x14002b5f0 (payload array).
//
// Declarations only; see marketwork_bst_b290.cpp for the offset-by-offset
// verification notes against the Application-x64.exe disassembly.
//
// These are the SAVE/LOAD side of the MarketWork AVL/BST family. 0x14002b290 is
// reached from MarketWork::Serialize (0x1400f7400 chain) ONLY when the IO::Serializer
// reports version != 0 — i.e. this pair is the **v1 (current) save format**. Version 0
// saves take the legacy simple-BST path (0x1400296b0) and are migrated by
// MarketBst_a9f0; nothing in this file ever runs for a v0 stream.
//
// Node layout (0x38 bytes, MarketWork's extended BST node — NOT the 0x20 node the
// other Work classes use). Byte offsets, all confirmed against the disassembly:
//   +0x00 key       InternalBasePtr (8 bytes), also the unsigned BST sort key
//   +0x08 payload   dynamic array: { void* ptr@+0x08, int32 count@+0x10, int32 cap@+0x14 }
//   +0x18 parent
//   +0x20 left
//   +0x28 right
//   +0x30 balance   int32 (AVL balance factor / height — zeroed on create)
//
// Tree header layout (what `tree` points at; it is MarketWork+0x48):
//   +0x00 root   node*
//   +0x08 count  int32
//
// On-disk record layout written/read by MarketBst_b290 (v1):
//   int32                          nodeCount
//   repeated nodeCount times:
//     InternalBasePtr              key            (Object::InternalBasePtr::Serialize)
//     int32                        payloadCount
//     repeated payloadCount times:
//       ExternalPtr/string-table   ref            (0x140002340)
//       int32                      value          (element is 0x10 bytes in memory)
// The per-node payload block is exactly what MarketBst_b5f0 reads/writes.
#pragma once

#include <cstdint>
#include "Work/MarketWork.h"   // MarketWork::ClaimNode / CarryList
namespace IO { class Serializer; }

extern "C" {

// 0x14002b5f0 — serialize one 16-byte-stride payload dynamic array.
//   arr        [RCX] : { ptr@+0x00, count@+0x08 (int32), cap@+0x0C (int32) }
//   serializer [RDX] : IO::Serializer*
// On load, resizes `arr` to the stored count before reading elements.
void MarketBst_b5f0(MarketWork::CarryList* arr, IO::Serializer* serializer);

// 0x14002b290 — serialize the whole market BST (v1 format).
//   tree       [RCX] : tree header { root@+0x00, count@+0x08 (int32) }
//   serializer [RDX] : IO::Serializer*
// Load: reads nodeCount, then inserts each (key, payload) pair, AVL-retracing via
//       0x140025920. Save: in-order walk, writing each node's key + payload block.
void MarketBst_b290(MarketWork::ClaimNode** tree, IO::Serializer* serializer);

} // extern "C"
