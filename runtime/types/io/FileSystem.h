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
struct PackageFileView {
    uint8_t  _unknown[0xA8];
    uint32_t packfileId;   // +0xA8
};
static_assert(offsetof(PackageFileView, packfileId) == 0xA8,
              "GetPackfileId 0x1800ab050: `*(uint*)(pkg + 0xa8)` on the resolved match");

// ---- IO::FileSystem --------------------------------------------------------
// Full-size fields at +0x00/+0x08 are RtSystem::String, NOT RtSystem::Filename:
// retail casts the pointer at the call site (`(Filename*)(this+8)`), it never
// declares the field as one. Filename and String are layout-identical (see
// runtime/types/system/Filename.h), so the cast is free either way; the type
// used here just matches what the field actually is.
struct FileSystem {
    RtSystem::String sourceDirectory;    // +0x00 — GetSourceDirectory's `(Filename*)this`
    RtSystem::String dataDirectory;      // +0x08 — GetDataDirectory's `this+8`; GetPackfileId's prefix
    uint8_t          _unknown10[0x18 - 0x10];
    uint8_t          existsInPackfileGate; // +0x18 — Exists()'s `CMP byte [RCX+0x18],0`
    uint8_t          platformExistsGate;   // +0x19 — Exists()'s `CMP byte [RBX+0x19],0`
    uint8_t          _unknown1A[0x20 - 0x1A];
    PackageFileView** packfiles;         // +0x20 — GetPackfileId's `*(this+0x20)`, array of PackageFile*
    int32_t          packfileCount;      // +0x28 — read by ModProbe_DumpPackageIndex, not by this lane
    uint8_t          _unknown2C[0x40 - 0x2C];  // includes the +0x30/+0x38 filename array+count.
                                                // NOT unread: +0x38 is the flag
                                                // IO::FileSystem::AreUserPackFilesEnabled
                                                // (0x1800acfc0) returns, and Achievement::
                                                // AchievementManager::Update reads it EVERY TICK
                                                // as the "mods are mounted, do not award" gate --
                                                // the mechanism behind BANISHED_ACH_IGNORE_MODS.
                                                // (Wave 5 lane B. This comment previously said
                                                // "nothing here reads" it, which was true of this
                                                // file and read as true of the tree.)
    PackfileIndexNode* packfileIndexRoot; // +0x40 — GetPackfileId's `*(this+0x40)`
};
static_assert(offsetof(FileSystem, sourceDirectory)      == 0x00, "see field comment");
static_assert(offsetof(FileSystem, dataDirectory)        == 0x08, "see field comment");
static_assert(offsetof(FileSystem, existsInPackfileGate) == 0x18, "see field comment");
static_assert(offsetof(FileSystem, platformExistsGate)   == 0x19, "see field comment");
static_assert(offsetof(FileSystem, packfiles)            == 0x20, "see field comment");
static_assert(offsetof(FileSystem, packfileCount)        == 0x28, "see field comment");
static_assert(offsetof(FileSystem, packfileIndexRoot)    == 0x40, "see field comment");

}  // namespace RtIO
