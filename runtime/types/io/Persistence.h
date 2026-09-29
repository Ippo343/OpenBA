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
#include <type_traits>   // B.3.4 PACKAGE+FS: the "IO::Persistence has no vptr" pins

#include "../system/String.h"
#include "../system/Filename.h"   // B.3.4: GetSaveFilename returns System::Filename

namespace RtIo {

// Defined in full at the bottom of this header; Persistence holds an array of
// them at +0x80 and ?GetSaveInfo@ returns one BY VALUE.
struct SaveGameInfo;

// One row of the block-descriptor array at +0x60 (stride 0xc: `ADD RCX,0xc`
// in ?SetupLoad@SavedState@Save@@ 0x1800c0cb7). The SAVE enqueue (0x1800b75c0)
// writes all three — +0x00 = the block's id, +0x04 = the running byte offset,
// +0x08 = the block's size — and SetupLoad matches +0x00 against
// SavedState::_id and returns +0x08.
struct BlockDesc {
    uint32_t id;       // +0x00
    uint32_t offset;   // +0x04
    uint32_t size;     // +0x08
};
static_assert(offsetof(BlockDesc, offset) == 0x04, "BlockDesc.offset @ 0x04");
static_assert(offsetof(BlockDesc, size)   == 0x08, "BlockDesc.size @ 0x08");
static_assert(sizeof(BlockDesc) == 0xc, "the +0x60 array strides 0xc");

struct Persistence {
    void*            settingsMapRoot;   // +0x00 — Set<Pair<String,String>> root
    int32_t          settingsMapCount;  // +0x08 — see below (Wave 9 lane BOOT2)
    uint8_t          _pad0c[0x04];      // +0x0C — NOT pinned

    // ---- the embedded worker, +0x10 .. +0x7F (Wave 19 lane PERSIST) --------
    // ??0Persistence@IO@@QEAA@XZ (0x1800b8020) runs
    // `??0Thread@System@@QEAA@I@Z(this+0x10, 0x10000)` and then stamps a
    // derived vtable (0x180155860) over `this+0x10`, so IO::Persistence
    // EMBEDS a Thread-derived save/load worker at +0x10 rather than being
    // one. Every offset below was read twice, once Persistence-relative in a
    // public entry point and once worker-relative (rdi = this+0x10) in the
    // private enqueue helpers, and the two agree:
    //
    //   field     Persistence-rel reader        worker-rel reader
    //   flags     0x1800b7c60 testb $1,0x18     —
    //   workEvt   0x1800b7911 mov 0x28(rbx)     0x1800b7428 mov 0x18(rdi)
    //   doneEvt   0x1800b791b mov 0x38(rbx)     0x1800b742c mov 0x28(rdi)
    //   filename  0x1800b78f7 mov ..,0x40(rbx)  0x1800b73f0 mov ..,0x30(rdi)
    //   status    0x1800b7903 movl $0,0x48(rbx) 0x1800b7425 mov esi,0x38(rdi)
    //   command   0x1800b78b7 movl $3,0x4c(rbx) 0x1800b741e movl $2,0x3c(rdi)
    //
    // The `flags` bit-0 test is the FIRST instruction of all seven public
    // entry points this lane ports; retail treats it as "the worker is dead /
    // persistence unavailable" and refuses the request. It is NOT a
    // busy flag: BeginGetSaveGameList (0x1800b7b30) tests it too and issues
    // straight away.
    void*            workerVtable;      // +0x10 — 0x180155860
    uint32_t         flags;             // +0x18 — bit 0 == unavailable
    uint8_t          _pad1c[0x04];      // +0x1C — NOT pinned
    // +0x20 and +0x30 were `_pad` until the WAVE 19 lane PERSIST post-report
    // tranche, and what pinned them is the WORKER DESTRUCTOR, 0x1800b7740 —
    // the one function this lane held ??1Persistence@ back on without ever
    // reading. Its tail is four CloseHandle calls on consecutive fields, which
    // is what says all four of these are HANDLEs and not three plus a spare:
    //     1800b77b6  SetEvent            ( [worker+0x20] )  == this+0x30
    //     1800b77c3  WaitForSingleObject ( [worker+0x10], INFINITE ) == this+0x20
    //     1800b77cd  CloseHandle ( [worker+0x10] )          == this+0x20
    //     1800b77d7  CloseHandle ( [worker+0x18] )          == this+0x28
    //     1800b77e1  CloseHandle ( [worker+0x20] )          == this+0x30
    //     1800b77f0  CloseHandle ( [worker+0x28] )  (tail)  == this+0x38
    // The WaitForSingleObject target is the one that is waited on and closed
    // FIRST, which is the thread itself; the other three are the events the
    // issue path already named. Import names read out of the DLL's own import
    // directory (0x18014b3c0 WaitForSingleObject, 0x18014b3b8 CloseHandle),
    // not guessed from the call shape.
    void*            threadHandle;      // +0x20 — HANDLE, WaitForSingleObject(INFINITE)
    void*            workEvent;         // +0x28 — HANDLE, SetEvent  (wake worker)
    void*            readyEvent;        // +0x30 — HANDLE, SetEvent  (in the dtor only)
    void*            doneEvent;         // +0x38 — HANDLE, ResetEvent(before issue)
    RtSystem::String pendingFilename;   // +0x40 — request filename
    int32_t          status;            // +0x48 — ?GetStatus@ (0x1800b7c40) reads this
    int32_t          command;           // +0x4C — 1 load · 2 delete · 3 info · 4 list
    int32_t          blockCount;        // +0x50 — int arg of BeginGetSaveGameInfo

    // ---- the on-disk header triple, +0x54 .. +0x5F ------------------------
    // Pinned 2026-09-12 by the SAVE job (FUN_1800b6e00, worker-relative +0x44/
    // +0x48/+0x4c) and independently corroborated by the GET-SAVE-INFO job
    // (FUN_1800b65b0, same three worker-relative offsets, reading them BACK):
    // the SAVE job writes `fileHeader` and `magic` as the file's first 12
    // bytes (4 + 8, the 8 being magic:crc together) and stores the running
    // CRC-32 into `crc`; the GET-SAVE-INFO job reads the same 12 bytes off
    // disk into the SAME three fields and checks them (fileHeader's high
    // 16 bits against `blockCount` above, `magic` against the sentinel
    // 0x80808080, `crc` against a freshly recomputed checksum). Whoever
    // enqueues a SAVE (BeginSaveGame, not yet ported) must pack
    // `fileHeader = version | (blockDescCount << 16)` and preset
    // `crc = 0xFFFFFFFF` before signalling the worker — neither job writes
    // those two on entry, only reads them.
    uint32_t         fileHeader;        // +0x54 — (version:16 | blockCount:16)
    uint32_t         magic;             // +0x58 — sentinel 0x80808080
    uint32_t         crc;               // +0x5C — running/verified CRC-32

    void*            blockDescData;     // +0x60 — Array<{id,?,size}>, stride 0xc
    int32_t          blockDescCount;    // +0x68 — ?GetDataSize@ (0x1800b7b80) walks these
    uint8_t          _pad6c[0x04];      // +0x6C — NOT pinned (Array capacity)
    void*            blockPtrData;      // +0x70 — Array<void*>
    int32_t          blockPtrCount;     // +0x78 — SetSize'd to blockDescCount at 0x1800b7514
    uint8_t          _pad7c[0x04];      // +0x7C — NOT pinned (Array capacity)

    // ---- the save list, +0x80 .. +0x8F ------------------------------------
    // ?GetSaveInfo@ (0x1800b7bd0) is `((SaveGameInfo*)this[0x80])[index]` with
    // `shl $4` for the stride; ?GetSaveCount@ (0x1800b7c60) returns the dword
    // at +0x88 and nothing else. Neither bounds-checks.
    SaveGameInfo*    saves;             // +0x80
    int32_t          saveCount;         // +0x88
    uint8_t          _pad8c[0x04];      // +0x8C — NOT pinned (Array capacity)

    RtSystem::String savePath;          // +0x90
};
static_assert(offsetof(Persistence, settingsMapCount) == 0x08,
              "Set<Pair<String,String>>::_count must sit at +0x08 (Map_Clear, retail 0x18000f7d0)");
static_assert(offsetof(Persistence, flags)           == 0x18, "flags @ 0x18");
static_assert(offsetof(Persistence, threadHandle)    == 0x20, "threadHandle @ 0x20 (~worker 0x1800b77c3)");
static_assert(offsetof(Persistence, workEvent)       == 0x28, "workEvent @ 0x28");
static_assert(offsetof(Persistence, readyEvent)      == 0x30, "readyEvent @ 0x30 (~worker 0x1800b77b6)");
static_assert(offsetof(Persistence, doneEvent)       == 0x38, "doneEvent @ 0x38");
static_assert(offsetof(Persistence, pendingFilename) == 0x40, "pendingFilename @ 0x40");
static_assert(offsetof(Persistence, status)          == 0x48, "status @ 0x48");
static_assert(offsetof(Persistence, command)         == 0x4c, "command @ 0x4c");
static_assert(offsetof(Persistence, blockCount)      == 0x50, "blockCount @ 0x50");
static_assert(offsetof(Persistence, fileHeader)      == 0x54, "fileHeader @ 0x54 (SAVE job worker+0x44)");
static_assert(offsetof(Persistence, magic)           == 0x58, "magic @ 0x58 (SAVE job worker+0x48)");
static_assert(offsetof(Persistence, crc)             == 0x5c, "crc @ 0x5c (SAVE job worker+0x4c)");
static_assert(offsetof(Persistence, blockDescData)   == 0x60, "blockDescData @ 0x60");
static_assert(offsetof(Persistence, blockDescCount)  == 0x68, "blockDescCount @ 0x68");
static_assert(offsetof(Persistence, blockPtrData)    == 0x70, "blockPtrData @ 0x70");
static_assert(offsetof(Persistence, blockPtrCount)   == 0x78, "blockPtrCount @ 0x78");
static_assert(offsetof(Persistence, saves)           == 0x80, "saves @ 0x80");
static_assert(offsetof(Persistence, saveCount)       == 0x88, "saveCount @ 0x88");
static_assert(offsetof(Persistence, savePath) == 0x90,
              "IO::Persistence::_savePath must sit at +0x90 (GetSavePath, retail 0x180007ad0)");

// ----------------------------------------------------------------------------
// The settings map's node.
// ----------------------------------------------------------------------------
// Map_Find returns a node pointer whose first two qwords are the key and
// value Strings (Persistence::Get reads `*(node+8)` as the value's `char*`;
// Persistence::Set writes through the same offset). The node continues with
// parent/left/right pointers at +0x10/+0x18/+0x20 — the exact pattern
// WAVE3_LANE_A.md measured for Set<Pair<K,V>> generally (`parent @ align8(sizeof
// K + sizeof V)`, then left, then right).
//
// B.4 R-IO: the link fields are declared, each pinned by a reader in this
// DLL: `parent` by WriteSettings (0x1800b8a9c reads [node+0x10] as the parent
// in its in-order successor, 0x1800b8aa5 compares [parent+0x18] against the
// node), `left`/`right` by Map_Clear (0x18000f7f4 recurses on [node+0x18],
// 0x18000f7fd on [node+0x20]). The two 16-bit AVL words at +0x28/+0x2a are
// runtime/src/collection/set_avl.h's (pinned there by the node allocator and
// the rotations); they are declared for the size and never read here.
//
// The key and value are the real `System::String`. No SettingsMapNode is ever
// constructed or destroyed as a C++ object: the nodes are raw memory that
// Map_Insert (rtavl::StringMapInsert) and Map_Clear (rtsetclear::Clear) own,
// and this header only views them, so the class type adds no ctor or dtor
// call anywhere. What it adds is `operator=` on the value, which IS the
// `String::operator=` (0x1800c9810) Persistence::Set and ReadSettings inline.
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
    System::String   key;      // +0x00
    System::String   value;    // +0x08
    SettingsMapNode* parent;   // +0x10
    SettingsMapNode* left;     // +0x18
    SettingsMapNode* right;    // +0x20
    int16_t          balance;  // +0x28
    int16_t          height;   // +0x2a
    uint32_t         pad2c;    // +0x2c
};
static_assert(offsetof(SettingsMapNode, value)  == 0x08, "value @ +0x08 (Persistence::Get)");
static_assert(offsetof(SettingsMapNode, parent) == 0x10, "parent @ +0x10 (0x1800b8a9c)");
static_assert(offsetof(SettingsMapNode, left)   == 0x18, "left @ +0x18 (0x18000f7f4)");
static_assert(offsetof(SettingsMapNode, right)  == 0x20, "right @ +0x20 (0x18000f7fd)");
static_assert(sizeof(SettingsMapNode) == 0x30, "the Set node set_avl.h pins");

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

// ============================================================================
// IO::Persistence::SaveGameInfo — 0x10 bytes, RETURNED BY VALUE.
// ----------------------------------------------------------------------------
// The NAME is not a guess: `?GetSaveInfo@Persistence@IO@@QEBA?AUSaveGameInfo@12@H@Z`
// (retail 0x1800b7bd0, ordinal 2888) spells it in full, and `?A` says the
// return is by value — so the real ABI is
// `GetSaveInfo(this, SaveGameInfo* hiddenSlot, int index)` with the index in
// R8D, which is what 0x1800c1fe3..0x1800c1fed does register for register.
// A typedef that omits that slot is a wild write that still returns the right
// answer (reference_hidden_retslot_wild_write.md).
//
// BOTH FIELDS ARE PINNED BY ONE READER, ?UpdateList@SaveLoad@Save@@ state 1:
//   +0x00  1800c1ff5  MOV RCX,[RAX]    -> copied into SaveInfo+0x30, which the
//                     qsort comparator at 0x1800c3db0 then compares UNSIGNED
//                     and orders descending. Named `sortKey` for what it is
//                     used as; see the SaveInfo banner in ../save/SaveLoad.h.
//   +0x08  1800c202b  MOV RAX,[R15+8]  -> refcount-assigned into SaveInfo+0x38,
//                     the display name. That it is a System::String is settled
//                     by the caller destroying it with the refcount idiom at
//                     0x1800c203b once it has copied it out.
// Nothing reads past +0x10, and the caller's by-value temporary destructor
// touches only +0x08, so 0x10 is the whole struct.
// ============================================================================
// *** +0x00 SETTLED 2026-09-12 (batch 5), FROM ITS PRODUCER. *** The comment
// above named it `sortKey` "for what it is used as", from the CONSUMER side
// only. The producer is the worker's command-4 job (retail 0x1800b6290,
// runtime/src/io/persistence_savelist_job.cpp), which fills it with
// `FileSystem::GetFileTimeStamp(<the .sav path>)` — a raw Win32 FILETIME, i.e.
// 100 ns ticks since 1601-01-01 UTC, which is exactly why an UNSIGNED
// DESCENDING sort puts the most recently written save first. The name is kept
// (it is still what the comparator uses it for) and the type is now known.
//
// B.4 R-IO: `name` is the real `System::String`. Every holder of a
// SaveGameInfo is either a C++ object whose destructor IS retail's release
// (the by-value temporaries in UpdateList state 1 and the command-4 job) or an
// element of Persistence's `Array<SaveGameInfo>` at +0x80, whose storage the
// hand-derived kSpec_SaveGameInfo row manages by offset (array.cpp), never
// through this type's destructor.
struct SaveGameInfo {
    uint64_t         sortKey;  // +0x00 — a FILETIME; see the note above
    System::String   name;     // +0x08
};
static_assert(offsetof(SaveGameInfo, name) == 0x08, "SaveGameInfo.name @ 0x08");
static_assert(sizeof(SaveGameInfo) == 0x10, "sizeof(IO::Persistence::SaveGameInfo) == 0x10");

}  // namespace RtIo

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 6 — the REAL `IO::Persistence`, PARTIAL
// ============================================================================
//
// Design: B34_RULES.md §1.5 rules 1 and 3, §4.3; B34_BRIEFS.md §E.4.
//
// *** NOTE THE POD NAMESPACE IN THIS FILE IS `RtIo`, LOWERCASE o *** — not
// `RtIO` as in `runtime/types/io/FileSystem.h`. B34_RULES §7.2's grep hazard is
// spelled against whichever one the file actually uses, and this file uses
// `RtIo` (as `Registry.h` does).
//
// *** IT HAS NO VTABLE OF ITS OWN. *** There is no `??_7Persistence@IO@@6B@` in
// the retail export table, no row for it in `standalone/rt_data_mint_tables.inc`
// and no `deleting_dtor.cpp` row. The ONLY Persistence row in the mint tables is
// the SINGLETON POINTER `?_singleton@Persistence@IO@@0PEAV12@EA` (0x1801fc6b8),
// which is a data symbol and not a table. Every exported member is
// `QEAA`/`QEBA`/`AEBA`. So, exactly as for `IO::Serializer`, `IO::Registry` and
// `IO::FileSystem`, converting a member here — the destructor included — cannot
// change which table any live object wears.
//
// *** THE VPTR AT +0x10 BELONGS TO THE EMBEDDED WORKER, NOT TO THIS CLASS. ***
// `??0Persistence@IO@@QEAA@XZ` (0x1800b8020) runs
// `??0Thread@System@@QEAA@I@Z(this+0x10, 0x10000)` and then stamps the derived
// worker table 0x180155860 over `this+0x10` — the field comment above says so
// and pins it. So `IO::Persistence` EMBEDS a `System::Thread`-derived save/load
// worker at +0x10; it does not derive from one, and +0x00 is a tree root, not a
// vptr. `_workerVtable` therefore stays a plain `void*` FIELD. ENDGAME B.3.9
// IO (W2): the worker is a real class, `IO::PersistenceWorker`
// (PersistenceWorker.h) -- `engine/singleton_ctors.cpp` placement-constructs it
// at +0x10 and `~Persistence` ends it directly; the by-address vtable stamps
// (0x180155860, 0x180155f20) are the compiler's own stores now.
//
// *** THIS DECLARATION IS DELIBERATELY PARTIAL AND CARRIES NO `sizeof` ASSERT
// *** AGAINST A LITERAL, AND THAT IS THIS HEADER'S OWN STANDING RULE. *** The
// POD above has none because NO function in the DLL enumerates Persistence's
// members — contrast `IO::Registry`, whose member-wise copy constructor pins
// 0x18 exactly and whose header says so explicitly. +0x0C, +0x1C, +0x6C, +0x7C
// and +0x8C are UNKNOWN and stay opaque padding, and the header's top banner's
// instruction stands: do not extend this struct from a decompile without a
// fresh `static_assert`. A class MAY keep opaque padding; it may NEVER invent a
// field. Every offset below is the POD's, cross-asserted against it, and the one
// `sizeof` assert here is an EQUALITY WITH THE POD — not a literal — which is
// what "the same bytes" means when the total is not independently pinned.
//
// *** THE TWO `RtSystem::String` MEMBERS STAY PODs. *** `_pendingFilename`
// (+0x40) is released by the WORKER's destructor (0x1800b7740,
// `persistence_worker_win.cpp`, which is not this class), and `_savePath`
// (+0x90) is released BY HAND by `~Persistence` itself. Retyping either to
// `System::String` would add an implicit member destructor running IN ADDITION
// to those hand-written releases — the B31 `RtSystem::String` double-free.
// (`RtIo::SettingsMapNode`'s key and value are `System::String` since B.4:
// see that struct -- no node is ever a C++ object, so no ctor/dtor is added.)
//
// *** THERE IS NO CONSTRUCTOR. *** `??0Persistence@IO@@QEAA@XZ` (0x1800b8020)
// is NOT ported here: it constructs the embedded `System::Thread` and stamps the
// worker table, and both belong to step 4b and to `engine/singleton_ctors.cpp`.
// The DESTRUCTOR does convert, because its door (0x1800b7fb0) is in
// `persistence_saves.cpp`, which IS in this block's file set, and because the
// class has no vtable to swap under anybody.
//
// MEMBERS, by the file their door lives in:
//   persistence.cpp              Get(int&), Get(bool&), Set(int), Set(bool),
//                                GetSavePath
//   persistence_registry_win.cpp ReadSettings, WriteSettings
//   persistence_saves.cpp        GetSaveCount, GetSaveInfo, GetSaveFilename,
//                                BeginDelete, BeginLoad, ~Persistence
//   persistence_saves_win.cpp    BeginGetSaveGameInfo
//   persistence_startup.cpp      Startup
//
// NOT MEMBERS, AND NOT CONVERTED: every `RtIo_PersistenceWorker_*` body — the
// enqueue helpers, `Run`, the save / load / list / info jobs and the worker
// destructor. Their receiver is `&p->_workerVtable`, i.e. the EMBEDDED worker,
// not the Persistence; they are `System::Thread`'s business and they are
// untouched.
// ============================================================================

namespace IO {

class Persistence {
public:
    // ?Get@Persistence@IO@@QEBA_NPEBD0AEAH@Z                 retail 0x1800b81e0
    bool Get(const char* section, const char* key, int& out) const;
    // ?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z                retail 0x1800b8290
    bool Get(const char* section, const char* key, bool& out) const;
    // ?Set@Persistence@IO@@QEAAXPEBD0H@Z                     retail 0x1800b80c0
    void Set(const char* section, const char* key, int value);
    // ?Set@Persistence@IO@@QEAAXPEBD0_N@Z                    retail 0x1800b8380
    void Set(const char* section, const char* key, bool value);
    // ?GetSavePath@Persistence@IO@@QEBA?AVString@System@@XZ  retail 0x180007ad0
    System::String GetSavePath() const;

    // ?ReadSettings@Persistence@IO@@QEAAXXZ                  retail 0x1800b84b0
    void ReadSettings();
    // ?WriteSettings@Persistence@IO@@QEBAXXZ                 retail 0x1800b88e0
    void WriteSettings() const;

    // ?Startup@Persistence@IO@@QEAA_NXZ                      retail 0x1800b7ee0
    bool Startup();

    // ?GetSaveCount@Persistence@IO@@QEBAHXZ                  retail 0x1800b7c60
    int GetSaveCount() const;
    // ?GetSaveInfo@Persistence@IO@@QEBA?AUSaveGameInfo@12@H@Z
    //                                                        retail 0x1800b7bd0
    // Retail returns `SaveGameInfo` BY VALUE (retslot, non-virtual: the
    // compiler's business). Its `name` is a real System::String, so the
    // by-value return is retail's copy-construct into the hidden slot.
    RtIo::SaveGameInfo GetSaveInfo(int index) const;
    // ?GetSaveFilename@Persistence@IO@@AEBA?AVFilename@System@@AEBVString@2@@Z
    //                                                        retail 0x1800b7c80
    // `AEBA` — PRIVATE in retail, and exported all the same (this DLL exports
    // its private members). Declared PUBLIC here for the reason `IO::FileSystem`
    // records for its two `Platform*` members: it has its OWN registered door
    // and a door is a free function.
    System::Filename GetSaveFilename(const System::String& name) const;
    // ?BeginDelete@Persistence@IO@@QEAA_NAEBVString@System@@@Z
    //                                                        retail 0x1800b7ab0
    bool BeginDelete(const System::String& name);
    // ?BeginLoad@Persistence@IO@@…                           retail 0x1800b7970
    bool BeginLoad(const System::String& name, int blockCount,
                   const RtIo::SaveBlock* blocks);
    // ?BeginGetSaveGameInfo@Persistence@IO@@QEAA_NAEBVString@System@@H@Z
    //                                                        retail 0x1800b7870
    bool BeginGetSaveGameInfo(const System::String& name, int count);

    // ??1Persistence@IO@@QEAA@XZ                             retail 0x1800b7fb0
    // A REAL destructor — see the banner. It is `QEAA`, non-virtual, and the
    // class has no vtable. There is deliberately NO constructor.
    ~Persistence();

    // ---- the pinned fields, at retail's offsets ---------------------------
    // PUBLIC for the same reason `IO::Registry`'s and `IO::FileSystem`'s are:
    // the unconverted worker bodies and the exe-side callers reach these
    // offsets through `RtIo::Persistence`, and `offsetof` on them has to be
    // well formed for the pins below. Field for field with the POD above, whose
    // per-offset evidence comments are not repeated here.
    RtIo::SettingsMapNode* _settingsMapRoot; // +0x00 — the Set<Pair<String,String>> root
    int32_t             _settingsMapCount;   // +0x08
    uint8_t             _pad0c[0x04];        // +0x0C — NOT pinned
    void*               _workerVtable;       // +0x10 — the EMBEDDED worker's vptr
    uint32_t            _flags;              // +0x18 — bit 0 == unavailable
    uint8_t             _pad1c[0x04];        // +0x1C — NOT pinned
    void*               _threadHandle;       // +0x20
    void*               _workEvent;          // +0x28
    void*               _readyEvent;         // +0x30
    void*               _doneEvent;          // +0x38
    RtSystem::String    _pendingFilename;    // +0x40 — POD; the WORKER frees it
    int32_t             _status;             // +0x48
    int32_t             _command;            // +0x4C
    int32_t             _blockCount;         // +0x50
    uint32_t            _fileHeader;         // +0x54
    uint32_t            _magic;              // +0x58
    uint32_t            _crc;                // +0x5C
    RtIo::BlockDesc*    _blockDescData;      // +0x60  Array<BlockDesc>._data
    int32_t             _blockDescCount;     // +0x68
    uint8_t             _pad6c[0x04];        // +0x6C — NOT pinned
    void*               _blockPtrData;       // +0x70
    int32_t             _blockPtrCount;      // +0x78
    uint8_t             _pad7c[0x04];        // +0x7C — NOT pinned
    RtIo::SaveGameInfo* _saves;              // +0x80
    int32_t             _saveCount;          // +0x88
    uint8_t             _pad8c[0x04];        // +0x8C — NOT pinned
    RtSystem::String    _savePath;           // +0x90 — POD; ~Persistence frees it

    // ?_singleton@Persistence@IO@@0PEAV12@EA (0x1801fc6b8) -- the class static
    // (ENDGAME B.3.9 IO, B39_RULES D7). Defined in persistence.cpp;
    // RtIoPortInit redirects the minted row onto it, so the name door, the
    // address door, the manifest publish and this member are the same eight
    // bytes. Written by the ??0Persistence door (engine/singleton_ctors.cpp).
    // PUBLIC while foreign readers still reach it (retail: private `0`).
    static Persistence* _singleton;
};

// The `sizeof` assert is an EQUALITY WITH THE POD, never a literal: nothing in
// the DLL enumerates this class's members, so the total is not independently
// pinned and writing a number here would be inventing one. What IS asserted is
// that the class and the POD describe the same bytes, offset for offset.
static_assert(sizeof(Persistence) == sizeof(RtIo::Persistence),
              "IO::Persistence and the RtIo::Persistence POD describe the same bytes");
static_assert(offsetof(Persistence, _settingsMapRoot)  == 0x00, "see field comment");
static_assert(offsetof(Persistence, _settingsMapCount) == 0x08, "see field comment");
static_assert(offsetof(Persistence, _workerVtable)     == 0x10, "see field comment");
static_assert(offsetof(Persistence, _flags)            == 0x18, "see field comment");
static_assert(offsetof(Persistence, _threadHandle)     == 0x20, "see field comment");
static_assert(offsetof(Persistence, _workEvent)        == 0x28, "see field comment");
static_assert(offsetof(Persistence, _readyEvent)       == 0x30, "see field comment");
static_assert(offsetof(Persistence, _doneEvent)        == 0x38, "see field comment");
static_assert(offsetof(Persistence, _pendingFilename)  == 0x40, "see field comment");
static_assert(offsetof(Persistence, _status)           == 0x48, "see field comment");
static_assert(offsetof(Persistence, _command)          == 0x4c, "see field comment");
static_assert(offsetof(Persistence, _blockCount)       == 0x50, "see field comment");
static_assert(offsetof(Persistence, _fileHeader)       == 0x54, "see field comment");
static_assert(offsetof(Persistence, _magic)            == 0x58, "see field comment");
static_assert(offsetof(Persistence, _crc)              == 0x5c, "see field comment");
static_assert(offsetof(Persistence, _blockDescData)    == 0x60, "see field comment");
static_assert(offsetof(Persistence, _blockDescCount)   == 0x68, "see field comment");
static_assert(offsetof(Persistence, _blockPtrData)     == 0x70, "see field comment");
static_assert(offsetof(Persistence, _blockPtrCount)    == 0x78, "see field comment");
static_assert(offsetof(Persistence, _saves)            == 0x80, "see field comment");
static_assert(offsetof(Persistence, _saveCount)        == 0x88, "see field comment");
static_assert(offsetof(Persistence, _savePath)         == 0x90, "see field comment");

static_assert(offsetof(Persistence, _settingsMapRoot)  == offsetof(RtIo::Persistence, settingsMapRoot),  "alias");
static_assert(offsetof(Persistence, _settingsMapCount) == offsetof(RtIo::Persistence, settingsMapCount), "alias");
static_assert(offsetof(Persistence, _workerVtable)     == offsetof(RtIo::Persistence, workerVtable),     "alias");
static_assert(offsetof(Persistence, _flags)            == offsetof(RtIo::Persistence, flags),            "alias");
static_assert(offsetof(Persistence, _threadHandle)     == offsetof(RtIo::Persistence, threadHandle),     "alias");
static_assert(offsetof(Persistence, _workEvent)        == offsetof(RtIo::Persistence, workEvent),        "alias");
static_assert(offsetof(Persistence, _readyEvent)       == offsetof(RtIo::Persistence, readyEvent),       "alias");
static_assert(offsetof(Persistence, _doneEvent)        == offsetof(RtIo::Persistence, doneEvent),        "alias");
static_assert(offsetof(Persistence, _pendingFilename)  == offsetof(RtIo::Persistence, pendingFilename),  "alias");
static_assert(offsetof(Persistence, _status)           == offsetof(RtIo::Persistence, status),           "alias");
static_assert(offsetof(Persistence, _command)          == offsetof(RtIo::Persistence, command),          "alias");
static_assert(offsetof(Persistence, _blockCount)       == offsetof(RtIo::Persistence, blockCount),       "alias");
static_assert(offsetof(Persistence, _fileHeader)       == offsetof(RtIo::Persistence, fileHeader),       "alias");
static_assert(offsetof(Persistence, _magic)            == offsetof(RtIo::Persistence, magic),            "alias");
static_assert(offsetof(Persistence, _crc)              == offsetof(RtIo::Persistence, crc),              "alias");
static_assert(offsetof(Persistence, _blockDescData)    == offsetof(RtIo::Persistence, blockDescData),    "alias");
static_assert(offsetof(Persistence, _blockDescCount)   == offsetof(RtIo::Persistence, blockDescCount),   "alias");
static_assert(offsetof(Persistence, _blockPtrData)     == offsetof(RtIo::Persistence, blockPtrData),     "alias");
static_assert(offsetof(Persistence, _blockPtrCount)    == offsetof(RtIo::Persistence, blockPtrCount),    "alias");
static_assert(offsetof(Persistence, _saves)            == offsetof(RtIo::Persistence, saves),            "alias");
static_assert(offsetof(Persistence, _saveCount)        == offsetof(RtIo::Persistence, saveCount),        "alias");
static_assert(offsetof(Persistence, _savePath)         == offsetof(RtIo::Persistence, savePath),         "alias");

// The guard, stated as an assertion so a future `virtual` fails the build rather
// than the boot: a vptr would push every field eight bytes down, under fifteen
// doors and under every worker body that reaches `+0x10` as the worker.
static_assert(!std::is_polymorphic<Persistence>::value,
              "IO::Persistence must never acquire a vtable: retail has none for it, and the "
              "vptr at +0x10 is the EMBEDDED worker's, not this class's");
static_assert(std::is_standard_layout<Persistence>::value,
              "the doors reach these fields through a reinterpret_cast");

}  // namespace IO
