#pragma once
// ============================================================================
// runtime/types/io/Persistence.h — IO::Persistence (Wave 4, lane C)
// ============================================================================
//
// Retail Runtime-x64.dll, image base 0x180000000. Layout is PARTIAL: only the
// two fields this lane's five ported functions actually touch are pinned.
// Everything else in the object is unknown and left as opaque padding — do
// not extend this struct from a decompile without a static_assert, and do not
// allocate a Persistence from this declaration; every instance this lane
// deals with is the live DLL singleton (`?_singleton@Persistence@IO@@0PEAV12@EA`,
// exported data, retail 0x1801fc6b8 — see WAVE4_LANE_C.md §"the singleton").
//
// FIELDS PINNED
// -------------
//   +0x00  root pointer of an in-memory Collection::Set<Pair<String,String>>
//          holding every "section+key" -> "value" entry ever Get/Set this
//          session (loaded from the registry by ReadSettings, flushed back by
//          WriteSettings — neither ported by this lane, see the wave doc).
//          Pinned by IO::Persistence::Get/Set (retail 0x1800b81e0, 0x1800b8290,
//          0x1800b80c0, 0x1800b8380), every one of which passes `this` itself
//          — not `this + N` — as the first argument to the shared internal
//          helpers `Map_Find` (retail 0x18002ace0) and `Map_Insert` (retail
//          0x18002a090), both of which read/write `*(void**)self` as the tree
//          root. Map_Find/Map_Insert are NOT owned by this lane (see below).
//   +0x90  System::String _savePath. Pinned by IO::Persistence::GetSavePath
//          (retail 0x180007ad0): `if (*(longlong*)(this+0x90) != 0) { copy
//          the pointer into the hidden return slot; ++refcount; }` — a plain
//          ref-counted read of a String field, nothing else.
//
//   +0x08  int32 element count of that same set. Added 2026-09-08 by Wave 9
//          lane BOOT2, which read `Map_Clear` (retail 0x18000f7d0, an unnamed
//          DLL internal, the first thing `ReadSettings` calls with `this` in
//          RCX). After it has recursively destroyed the tree it writes BOTH
//          roots of the container's state and nothing else:
//              XOR EAX,EAX
//              MOV qword ptr [RBX],RAX        ; +0x00 root  = null
//              MOV dword ptr [RBX+0x8],EAX    ; +0x08 count = 0   <-- a DWORD
//          The 8-vs-4 byte split is the evidence for the width; the empty-tree
//          arm at 0x18000f824 writes the identical pair. Nothing this lane
//          read pins +0x0C.
//
// Offsets 0x0C..0x8F are UNKNOWN. ReadSettings/WriteSettings construct their
// own `Registry` helper object on the STACK (`Registry::Registry(&local, 0)`),
// not through a field of `this`, so whatever else lives in that gap is not
// evidenced by any function this lane read. Held-back functions (see the wave
// doc) may need it; a future lane extending this struct must repin it with
// its own static_assert, not assume this comment's gap is empty.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../system/String.h"

namespace RtIo {

struct Persistence {
    void*            settingsMapRoot;   // +0x00 — Set<Pair<String,String>> root
    int32_t          settingsMapCount;  // +0x08 — see below (Wave 9 lane BOOT2)
    uint8_t          _unknown[0x84];    // +0x0C .. +0x8F — NOT pinned, NOT modeled
    RtSystem::String savePath;          // +0x90
};
static_assert(offsetof(Persistence, settingsMapCount) == 0x08,
              "Set<Pair<String,String>>::_count must sit at +0x08 (Map_Clear, retail 0x18000f7d0)");
static_assert(offsetof(Persistence, savePath) == 0x90,
              "IO::Persistence::_savePath must sit at +0x90 (GetSavePath, retail 0x180007ad0)");

// ----------------------------------------------------------------------------
// The settings map's node — a PREFIX view only.
// ----------------------------------------------------------------------------
// Map_Find returns a node pointer whose first two qwords are the key and
// value Strings (Persistence::Get reads `*(node+8)` as the value's `char*`;
// Persistence::Set writes through the same offset). The node continues past
// this with parent/left/right pointers at +0x10/+0x18/+0x20 — the exact
// pattern WAVE3_LANE_A.md measured for Set<Pair<K,V>> generally
// (`parent @ align8(sizeof K + sizeof V)`, then left, then right) — but this
// lane never walks, inserts, or rebalances a node by hand, so those fields
// are not declared here. Do not take sizeof(SettingsMapNode) as the real node
// size; it is a PREFIX overlay onto memory Map_Find/Map_Insert own.
//
// Map_Find (retail 0x18002ace0) and Map_Insert (retail 0x18002a090) are
// process-wide shared code, not Persistence-specific: MSVC folds every
// `Set<Pair<String,String>, ...>` instantiation in the DLL into these same
// two routines because both K and V are 8-byte String handles regardless of
// the conceptual map. They belong to Collection::Set, a class no lane has
// ported yet (PLAN.md §3.3, §4.3 risk 1), so this lane calls them BY ADDRESS
// as opaque engine dependencies — exactly PLAN §6 step 3 ("resolve deps") —
// rather than reimplementing AVL insert/find by hand, which would repeat the
// "one BST for two node layouts" trap WAVE3_LANE_A.md and
// reference_one_bst_find_two_node_layouts.md both warn about.
struct SettingsMapNode {
    RtSystem::String key;    // +0x00
    RtSystem::String value;  // +0x08
};
static_assert(sizeof(SettingsMapNode) == 0x10, "SettingsMapNode prefix must be exactly key+value, 0x10 bytes");

// ============================================================================
// IO::Persistence::SaveBlock — 0x10 bytes. One buffer handed to the save/load
// worker thread. Added by Wave 13, pinned by THREE independent readers rather
// than by one writer:
//
//   * ?UpdateSave@SaveLoad (0x1800c2c60..0x1800c2c88) builds an array of TWO
//     of them, entry 0 from the SavedState embedded in SaveLoad at +0x28 and
//     entry 1 from the heap SavedState at +0x40. The three loads per entry are
//     qword `data`, then dword `size`, then dword `id`, at +0x00/+0x08/+0x0c.
//   * ?UpdateLoadFromFile@SaveLoad (0x1800c25cc) takes ONE straight from
//     ?SetupLoad@SavedState@Save@@, whose mangled return type spells this
//     struct's full name — which is where the name here comes from, not a guess.
//   * The unexported worker-enqueue at 0x1800b7460 walks the array the caller
//     passes with `param_4 += 2` on an 8-byte pointer (stride 0x10) and matches
//     `*(int*)((char*)param_4 + 0xc)` against a per-slot id, which pins BOTH
//     the stride and the fact that +0x0c is the id and not part of the size.
//
// The field ORDER is deliberately data/size/id and not the SavedState order
// (id/size/data): these two structs are NOT the same layout, and UpdateSave's
// six loads per entry are what says so.
struct SaveBlock {
    void*    data;   // +0x00
    uint32_t size;   // +0x08
    uint32_t id;     // +0x0c
};
static_assert(offsetof(SaveBlock, size) == 0x08, "SaveBlock.size @ 0x08");
static_assert(offsetof(SaveBlock, id)   == 0x0c, "SaveBlock.id @ 0x0c");
static_assert(sizeof(SaveBlock) == 0x10, "sizeof(IO::Persistence::SaveBlock) == 0x10");

}  // namespace RtIo
