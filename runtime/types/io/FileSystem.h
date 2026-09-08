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
// Wave 9 lane PKG2 adds the eight fields the class's own accessors read. Each
// is pinned by the one-instruction body that reads it — every accessor named
// below is `MOV EAX,[RCX+off] ; RET` or the String AddRef idiom, so the offset
// is the whole function and there is nothing to misread.
struct PackageFileView {
    void*    _vtable;              // +0x00 — ??_7PackageFile@IO@@6B@, 0x180155700
    uint8_t  _unknown08[0x40 - 0x08];
    // +0x40/+0x48/+0x50 — three System::String fields, each returned by a
    // getter that AddRefs rather than copies:
    //   ?GetName@        0x18002532a `MOV RAX,qword ptr [RCX+0x40]`
    //   ?GetAuthor@      0x18002536a `MOV RAX,qword ptr [RCX+0x48]`
    //   ?GetDescription@ 0x1800253aa `MOV RAX,qword ptr [RCX+0x50]`
    // The destructor (0x1800b4540) releases +0x40..+0x60 with the inlined
    // refcount idiom, which is the independent second pin on all three.
    RtSystem::String name;         // +0x40
    RtSystem::String author;       // +0x48
    RtSystem::String description;  // +0x50
    uint8_t  _unknown58[0x68 - 0x58];
    int32_t  version;              // +0x68 — ?GetVersion@     0x1800253d0
    int32_t  userVersion;          // +0x6C — ?GetUserVersion@ 0x1800253e0
    // +0x70 — the SORT KEY, and the only field the packfile sort compares.
    // 0x1800264a0 is an introsort over `PackageFile*` whose entire comparator is
    // `MOV EAX,[RAX+0x70] ; CMP dword ptr [RCX+0x70],EAX`, and the constructor
    // seeds it with the qword 0x2710 (10000) — so an unread packfile sorts to a
    // fixed default and ReadPackfile is what makes one package outrank another.
    //
    // Lane PKG modelled this as a `uint64_t` because the constructor's store is
    // one qword. It is TWO dwords: `?GetPriority@` (0x1800254e0) reads the low
    // half and `?GetPlatform@` (0x1800253f0) reads `[RCX+0x74]`, the high half.
    // The qword store therefore seeds priority=0x2710 AND platform=0. Splitting
    // it changes no offset and no static_assert; nothing in the tree read the
    // field as a qword (checked by grep before the change).
    int32_t  priority;             // +0x70 — ?GetPriority@ 0x1800254e0
    int32_t  platform;             // +0x74 — ?GetPlatform@ 0x1800253f0
    // +0x78/+0x7C — the icon dimensions, and the ONLY two fields in this struct
    // the constructor does NOT initialise: its store list jumps 0x70 -> 0x80.
    // That is not an omission in the transcription, it is retail leaving them
    // to ReadPackfile, and it is why the constructor's byte oracle must not
    // compare them (reference_oracle_must_not_compare_unwritten_bytes.md).
    int32_t  iconWidth;            // +0x78 — ?GetIconWidth@  0x180025510
    int32_t  iconHeight;           // +0x7C — ?GetIconHeight@ 0x180025520
    uint8_t  _unknown80[0xA8 - 0x80];
    // +0xA8 — the package CRC, which is ALSO the packfile id. Two exported
    // readers hit the same four bytes: ?GetCRC@ (0x180025500) and
    // FileSystem::GetPackfileId (0x1800ab050). They are the same field, not two.
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
// One entry of the +0xB0 table. Stride 16, pinned three independent ways:
// GetFileName's `SHL RDX,0x4`, GetFileOffset's `[RAX + R8*8 + 0x8]` after
// `ADD R8,R8` (= index*16 + 8) and GetFileSize's `+ 0xc` (= index*16 + 12).
struct PackageFileEntry {
    RtSystem::String name;         // +0x00 — the Filename GetFileName AddRefs
    uint32_t         offset;       // +0x08 — ?GetFileOffset@ 0x1800254c0
    uint32_t         size;         // +0x0C — ?GetFileSize@   0x1800254a0
};
static_assert(sizeof(PackageFileEntry) == 16,
              "GetFileName 0x180025470: `SHL RDX,0x4` is the stride");
static_assert(offsetof(PackageFileEntry, offset) == 0x08,
              "GetFileOffset 0x1800254cd: `MOV EAX,dword ptr [RAX + R8*0x8 + 0x8]`");
static_assert(offsetof(PackageFileEntry, size)   == 0x0C,
              "GetFileSize 0x1800254ad: `MOV EAX,dword ptr [RAX + R8*0x8 + 0xc]`");

static_assert(offsetof(PackageFileView, name)        == 0x40,
              "GetName 0x18002532a: `MOV RAX,qword ptr [RCX+0x40]`");
static_assert(offsetof(PackageFileView, author)      == 0x48,
              "GetAuthor 0x18002536a: `MOV RAX,qword ptr [RCX+0x48]`");
static_assert(offsetof(PackageFileView, description) == 0x50,
              "GetDescription 0x1800253aa: `MOV RAX,qword ptr [RCX+0x50]`");
static_assert(offsetof(PackageFileView, version)     == 0x68,
              "GetVersion 0x1800253d0: `MOV EAX,dword ptr [RCX+0x68]`");
static_assert(offsetof(PackageFileView, userVersion) == 0x6C,
              "GetUserVersion 0x1800253e0: `MOV EAX,dword ptr [RCX+0x6c]`");
static_assert(offsetof(PackageFileView, priority)   == 0x70,
              "sort 0x1800264a0: `CMP dword ptr [RCX+0x70],EAX` is its whole comparator");
static_assert(offsetof(PackageFileView, platform)   == 0x74,
              "GetPlatform 0x1800253f0: `MOV EAX,dword ptr [RCX+0x74]`");
static_assert(offsetof(PackageFileView, iconWidth)  == 0x78,
              "GetIconWidth 0x180025510: `MOV EAX,dword ptr [RCX+0x78]`");
static_assert(offsetof(PackageFileView, iconHeight) == 0x7C,
              "GetIconHeight 0x180025520: `MOV EAX,dword ptr [RCX+0x7c]`");
static_assert(offsetof(PackageFileView, packfileId) == 0xA8,
              "GetPackfileId 0x1800ab050: `*(uint*)(pkg + 0xa8)` on the resolved match");
static_assert(offsetof(PackageFileView, overridable) == 0xAC,
              "LoadPackages 0x1800ab91e: `CMP byte ptr [RCX+0xac],0x0`");
static_assert(offsetof(PackageFileView, entries)    == 0xB0,
              "GetFileName 0x180025475: `ADD RDX,qword ptr [RCX+0xb0]`");
static_assert(offsetof(PackageFileView, entryCount) == 0xB8,
              "LoadPackages 0x1800ab889: `MOV EAX,dword ptr [RCX+0xb8]`");

// ---- IO::FileSystem::FileInfo ---------------------------------------------
// Moved here from runtime/src/io/filesystem_setupdirs_win.cpp by Wave 9 lane
// PKG2 rung 3, because a second translation unit now needs it and two
// namespace-scope `RtIO::FileInfo` definitions with different members would be
// a silent ODR violation rather than a compile error.
//
// **AND ONE FIELD IS CORRECTED IN THE MOVE.** Lane FS modelled +0x08 as a
// `uint64_t size`, which was a reasonable reading of the one instruction it had
// (`MOV qword [RBP-0x28],R15` zeroes +0x08 and +0x0C together). It is two
// separate dwords, and three independent sites say so:
//
//   * `?ReadFile@FileSystem@IO@@` reads +0x0C as a READ CURSOR and advances it
//     (`ADD dword [RDI+0xc],ESI` at 0x1800ace87, and again at 0x1800acf37);
//   * `?OpenFile@FileSystem@IO@@` writes +0x08 and +0x0C from two different
//     values in the same breath — `MOV [RDI+8],R9D` (the entry SIZE) then
//     `MOV [RDI+0xc],ECX` (the cursor, 0) at 0x1800acb94/98;
//   * the FileInfo COPY CTOR (0x18000c110) copies +0x00, +0x04, +0x08, +0x0C
//     and +0x10 as five separate DWORD moves.
//
// Nothing broke while it was wrong: lane FS's only reader is
// `static_cast<int>(static_cast<uint32_t>(fi.size))`, which takes the low dword
// and so happened to be right. It would not have stayed right for anyone who
// used the field as a size on a file opened from inside a package, where +0x0C
// is a live cursor rather than zero.
struct FileInfo {
    // +0x00 — WHAT KIND OF FILE THIS IS, and it is not a plain handle:
    //   -1  nothing opened
    //   -2  a LOOSE file; the real Win32 HANDLE is at +0x18
    //   otherwise a PACKED file, encoded as `(packIndex << 24) | fileIndex`
    //     (OpenFile 0x1800ac9c5: `MOV R8D,[RBX+8] ; SHL R8D,0x18 ; OR R8D,[RBX+0xc]`).
    // ReadFile discriminates with `CMP dword [RDX],-3 ; JA loose`, i.e. unsigned
    // -- so -1 and -2 both take the loose path -- and recovers the pack index as
    // the TOP BYTE (`MOVZX R10D,byte [RDX+3]`).
    int32_t  handle;
    int32_t  _unk04;      // +0x04 — never initialised by OpenFile; see the note
                          //         in filesystem_openfile_win.cpp on why the
                          //         oracle must exclude it.
    int32_t  size;        // +0x08 — byte size of the file or of the pack entry
    int32_t  cursor;      // +0x0C — READ POSITION, advanced by ReadFile
    int32_t  baseOffset;  // +0x10 — a pack entry's offset within the .pkg; 0 for loose
    int32_t  _pad14;      // +0x14 — not copied by the copy ctor
    int64_t  winHandle;   // +0x18 — the Win32 HANDLE, -1 when there is none
    RtSystem::String _name0;   // +0x20 — a Filename (the copy ctor is ??0Filename@)
    RtSystem::String _name1;   // +0x28
};
static_assert(sizeof(FileInfo) == 0x30, "FileInfo is 0x30 bytes (OpenFile's frame)");
static_assert(offsetof(FileInfo, size)       == 0x08,
              "OpenFile 0x1800acb94: `MOV dword ptr [RDI+0x8],R9D` (the entry size)");
static_assert(offsetof(FileInfo, cursor)     == 0x0C,
              "ReadFile 0x1800ace87: `ADD dword ptr [RDI+0xc],ESI`");
static_assert(offsetof(FileInfo, baseOffset) == 0x10,
              "OpenFile 0x1800acb9b: `MOV dword ptr [RDI+0x10],EDX` (the entry offset)");
static_assert(offsetof(FileInfo, winHandle)  == 0x18,
              "PlatformOpenFile 0x1800adddd: `MOV qword ptr [RBX+0x18],RAX` (CreateFileW)");
static_assert(offsetof(FileInfo, _name0)     == 0x20, "OpenFile: MOV [RBP-0x10],0");
static_assert(offsetof(FileInfo, _name1)     == 0x28, "OpenFile: MOV [RBP-0x8],0");

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
