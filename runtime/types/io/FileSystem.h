#pragma once
// ============================================================================
// runtime/types/io/FileSystem.h — IO::FileSystem binary layout
// ============================================================================
// Wave 4, lane D. Every offset below was read live in Ghidra on 2026-09-06
// against Runtime-x64.dll (image base 0x180000000) via the five functions
// this lane ports (filesystem.cpp): Exists, GetDataDirectory,
// GetSourceDirectory, GetPackfileId, and — indirectly, since it walks the
// same tree — nothing else. Only the fields those five touch are modelled;
// everything else is opaque padding.
//
// This is NOT the first documentation of this layout. Lane D's own
// code/Game/Dialog/mod_dialog_vtables.cpp (2026-09-05, ModProbe_DumpFileSystem
// / ModProbe_DumpPackageIndex) already recovered +0x08 dataDirectory, +0x18/
// +0x19 the two Exists() gates, +0x20/+0x28 the packfile array+count, +0x30/
// +0x38 a package-filename array+count (NOT read by anything in this file),
// and +0x40 the path->packfile BST root — all from reading the exe-side probe
// code, not from decompiling FileSystem's own methods. This header adds the
// one thing that comment never had: a static_assert per offset, each pinned
// by the actual instruction in the method that reads it.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../system/String.h"

namespace RtIO {

// ---- The path -> packfile index BST --------------------------------------
// Walked identically by GetPackfileId (0x1800ab050), ExistsInPackfile
// (0x1800ab180) and LoadPackages (0x1800ab460) — all three do `node = node[3]`
// / `node = node[4]` for the children and read `*(uint*)(node + 1)` for the
// packed pack/file index. Only GetPackfileId's walk is ported here.
struct PackfileIndexNode {
    const char*        key;        // +0x00 — dataDir-relative path, case-insensitive
    uint32_t           packIndex;  // +0x08 — low dword of the packed pair
    uint32_t           fileIndex;  // +0x0C — high dword; unread by GetPackfileId
    uint64_t           _unused10;  // +0x10 — touched by none of the three walks
    PackfileIndexNode* left;       // +0x18
    PackfileIndexNode* right;      // +0x20
};
static_assert(offsetof(PackfileIndexNode, packIndex) == 0x08,
              "GetPackfileId 0x1800ab050: `*(uint*)(puVar5 + 1)` on the found node");
static_assert(offsetof(PackfileIndexNode, left)  == 0x18,
              "GetPackfileId 0x1800ab050: `puVar5 = puVar5[3]` (left descent)");
static_assert(offsetof(PackfileIndexNode, right) == 0x20,
              "GetPackfileId 0x1800ab050: `puVar5 = puVar5[4]` (right descent)");

// ---- PackageFile — only the field GetPackfileId returns -------------------
// Wave 4 lane D modelled only +0xA8. Wave 9 lane PKG adds the four fields
// LoadPackages / ClosePackages touch; everything else is still opaque. Each
// offset below is pinned by the instruction that reads or writes it, and every
// one of them was also seen being INITIALISED by ??0PackageFile@IO@@QEAA@XZ
// (0x1800b4440), which is the independent second pin.
struct PackageFileView {
    void*    _vtable;              // +0x00 — ??_7PackageFile@IO@@6B@, 0x180155700
    uint8_t  _unknown08[0x70 - 0x08];
    // +0x70 — the SORT KEY, and the only field the packfile sort compares.
    // 0x1800264a0 is an introsort over `PackageFile*` whose entire comparator is
    // `MOV EAX,[RAX+0x70] ; CMP dword ptr [RCX+0x70],EAX`, and the constructor
    // seeds it with the qword 0x2710 (10000) — so an unread packfile sorts to a
    // fixed default and ReadPackfile is what makes one package outrank another.
    uint64_t priority;
    uint8_t  _unknown78[0xA8 - 0x78];
    uint32_t packfileId;           // +0xA8
    // +0xAC — the OVERRIDE flag, born 1 in the constructor
    // (`MOV byte ptr [RCX+0xac],0x1` at 0x1800b44dc). LoadPackages' index pass
    // reads it on the package that ALREADY owns a colliding path
    // (`CMP byte ptr [RCX+0xac],0x0` at 0x1800ab91e): nonzero means the later
    // package takes the entry over, zero means the incumbent keeps it. That is
    // the whole of the loose-file / .pkg / .pkm precedence rule.
    uint8_t  overridable;
    uint8_t  _unknownAD[0xB0 - 0xAD];
    // +0xB0/+0xB8 — the package's file table, array + count, stride 16.
    // The stride is not inferred: ?GetFileName@PackageFile@IO@@ (0x180025450)
    // is `MOVSXD RDX,R8D ; SHL RDX,0x4 ; ADD RDX,[RCX+0xb0]` and then a Filename
    // copy ctor from that address, so entry+0 is a Filename and the entries are
    // 16 bytes apart. LoadPackages reads the count as a dword at +0xB8
    // (`MOV EAX,dword ptr [RCX+0xb8]`).
    void*    entries;
    int32_t  entryCount;
};
static_assert(offsetof(PackageFileView, priority)   == 0x70,
              "sort 0x1800264a0: `CMP dword ptr [RCX+0x70],EAX` is its whole comparator");
static_assert(offsetof(PackageFileView, packfileId) == 0xA8,
              "GetPackfileId 0x1800ab050: `*(uint*)(pkg + 0xa8)` on the resolved match");
static_assert(offsetof(PackageFileView, overridable) == 0xAC,
              "LoadPackages 0x1800ab91e: `CMP byte ptr [RCX+0xac],0x0`");
static_assert(offsetof(PackageFileView, entries)    == 0xB0,
              "GetFileName 0x180025475: `ADD RDX,qword ptr [RCX+0xb0]`");
static_assert(offsetof(PackageFileView, entryCount) == 0xB8,
              "LoadPackages 0x1800ab889: `MOV EAX,dword ptr [RCX+0xb8]`");

// ---- IO::FileSystem --------------------------------------------------------
// Full-size fields at +0x00/+0x08 are RtSystem::String, NOT RtSystem::Filename:
// retail casts the pointer at the call site (`(Filename*)(this+8)`), it never
// declares the field as one. Filename and String are layout-identical (see
// runtime/types/system/Filename.h), so the cast is free either way; the type
// used here just matches what the field actually is.
struct FileSystem {
    RtSystem::String sourceDirectory;    // +0x00 — GetSourceDirectory's `(Filename*)this`
    RtSystem::String dataDirectory;      // +0x08 — GetDataDirectory's `this+8`; GetPackfileId's prefix
    // +0x10 — the PACK directory, and the third field
    // ?SetupDirectories@FileSystem@IO@@ (0x1800abd00) writes: it is seeded as
    // a copy of dataDirectory (retail's inlined String::operator= at
    // 0x1800ac1b0 with RSI = this+0x10, RDI = this+8) and then overridden by a
    // `-pathpkg` command-line switch. `?GetPackDirectory@FileSystem@IO@@`
    // (0x180007c10) is its reader. Named by Wave 9 lane FS; it was
    // `_unknown10[8]` before, and nothing anywhere read that padding.
    RtSystem::String packDirectory;
    uint8_t          existsInPackfileGate; // +0x18 — Exists()'s `CMP byte [RCX+0x18],0`
    uint8_t          platformExistsGate;   // +0x19 — Exists()'s `CMP byte [RBX+0x19],0`
    uint8_t          _unknown1A[0x20 - 0x1A];
    PackageFileView** packfiles;         // +0x20 — GetPackfileId's `*(this+0x20)`, array of PackageFile*
    int32_t          packfileCount;      // +0x28 — read by ModProbe_DumpPackageIndex, not by this lane
    // +0x2C — the packfiles array's CAPACITY. Named by Wave 9 lane PKG from
    // LoadPackages' two inlined array-growth loops, which read it
    // (`CMP R14D,dword ptr [RSI+0x2c]` at 0x1800ab5d4) and write it
    // (`MOV dword ptr [RSI+0x2c],EAX` at 0x1800ab5f0). So +0x20/+0x28/+0x2C is
    // a Collection::Array<PackageFile*> laid out exactly like every other
    // Array in this engine (_data / _size / _capacity — see
    // runtime/types/collection/Collection.h); retail INLINES the growth here
    // rather than calling Array::SetSize, which is why the three fields were
    // never named together before.
    int32_t          packfileCapacity;
    // +0x30/+0x38 — the USER (mod) pack filenames, array + count.
    //
    // CORRECTION, Wave 9 lane PKG: this comment used to call +0x38 "the flag
    // IO::FileSystem::AreUserPackFilesEnabled returns". It is not a flag, it is
    // an int COUNT, and the whole of that accessor (0x1800acfc0, four
    // instructions) is `XOR EAX,EAX / CMP dword ptr [RCX+0x38],EAX / SETG AL`
    // — i.e. it returns `count > 0`. Both readings agree on the gate that
    // BANISHED_ACH_IGNORE_MODS turns on, so nothing built on the old wording is
    // wrong; but LoadPackages' second loop uses +0x38 as a LOOP BOUND
    // (`CMP R14D,dword ptr [RSI+0x38]` at 0x1800ab864) while indexing +0x30,
    // and a bool there would have been a one-iteration loop.
    RtSystem::String* userPackFilenames;   // +0x30
    int32_t           userPackFilenameCount; // +0x38
    int32_t           _unknown3C;            // +0x3C — the matching capacity; unread here
    PackfileIndexNode* packfileIndexRoot; // +0x40 — GetPackfileId's `*(this+0x40)`
    // +0x48 — the node count of the BST rooted at +0x40, so +0x40/+0x48 is one
    // tree object. Pinned twice: the insert helper at 0x180025c70 takes
    // `this+0x40` as its tree and does `INC dword ptr [R14+0x8]` on every node
    // it creates, and ClosePackages zeroes it (`MOV dword ptr [RSI+0x8],EBP`
    // at 0x1800abbf0, with RSI = this+0x40).
    int32_t          packfileIndexCount;
};
static_assert(offsetof(FileSystem, sourceDirectory)      == 0x00, "see field comment");
static_assert(offsetof(FileSystem, dataDirectory)        == 0x08, "see field comment");
static_assert(offsetof(FileSystem, packDirectory)        == 0x10, "see field comment");
static_assert(offsetof(FileSystem, existsInPackfileGate) == 0x18, "see field comment");
static_assert(offsetof(FileSystem, platformExistsGate)   == 0x19, "see field comment");
static_assert(offsetof(FileSystem, packfiles)            == 0x20, "see field comment");
static_assert(offsetof(FileSystem, packfileCount)        == 0x28, "see field comment");
static_assert(offsetof(FileSystem, packfileCapacity)      == 0x2c, "see field comment");
static_assert(offsetof(FileSystem, userPackFilenames)    == 0x30, "see field comment");
static_assert(offsetof(FileSystem, userPackFilenameCount) == 0x38, "see field comment");
static_assert(offsetof(FileSystem, packfileIndexRoot)    == 0x40, "see field comment");
static_assert(offsetof(FileSystem, packfileIndexCount)   == 0x48, "see field comment");

}  // namespace RtIO
