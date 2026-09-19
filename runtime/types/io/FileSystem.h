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
#include <type_traits>   // B.3.4 PACKAGE+FS: the "IO::FileSystem has no vptr" pins

#include "../system/String.h"
#include "../system/Filename.h"   // B.3.4: IO::FileSystem returns System::Filename by value

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
    // +0x10 CORRECTED 2026-09-08 (Wave 9 lane COLL). It is the AVL PARENT
    //       pointer, not an unused field: 0x180008840 writes it
    //       (`MOV [RSI+0x10],RDI` with RDI == the parent) and the retrace
    //       0x180025e10 reads it (`MOV RSI,[RAX+0x10]`) to walk to the root.
    //       "Touched by none of the three walks" was true of the three READ
    //       walks lane PKG had, and false of the tree — the mirror of
    //       reference_never_written_needs_by_whom.md: a field is untouched BY
    //       SOMEBODY, never in general.
    PackfileIndexNode* parent;     // +0x10
    PackfileIndexNode* left;       // +0x18
    PackfileIndexNode* right;      // +0x20
    // +0x28/+0x2a MEASURED 2026-09-08. The node is a full AVL node, not a plain
    //       BST node: 0x180025e10 writes a SIGNED 16-bit balance at +0x28 and a
    //       16-bit height at +0x2a, and 0x180008840 zeroes both with one dword
    //       store. sizeof is 0x30 (0x180008520: `MOV EDX,0x30 / LEA
    //       R8D,[RDX-0x28]` -> Allocate(0x30, align 8)) — the same node
    //       header runtime/src/collection/set_avl.h describes.
    int16_t            balance;    // +0x28
    int16_t            height;     // +0x2a
    uint32_t           _pad2c;     // +0x2c
};
static_assert(sizeof(PackfileIndexNode) == 0x30,
              "0x180008520 allocates 0x30 bytes, align 8");
static_assert(offsetof(PackfileIndexNode, parent) == 0x10,
              "0x180008840 MOV [RSI+0x10],RDI");
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
    // +0x58 and +0x60 — TWO MORE System::String fields, pinned Wave 25 lane
    // BODY by the destructor (0x1800b4540): it releases FIVE slots with the
    // inlined refcount idiom, at +0x60, +0x58, +0x50, +0x48 and +0x40, in that
    // (descending) order. The idiom is unmistakable — `DEC [RAX-4]` on the
    // refcount word, the freeing arm storing `data - 8` back into the slot and
    // the sharing arm nulling it — so the TYPE of these two is settled.
    //
    // THEY ARE DELIBERATELY NOT NAMED. A destructor pins a field's TYPE and
    // says nothing whatever about its MEANING, and no reader of either offset
    // has been found. Inventing `url`/`tag`/`category` here would put a guess
    // where the header everywhere else carries an instruction.
    RtSystem::String _string58;    // +0x58 — purpose unknown; type from ~PackageFile
    RtSystem::String _string60;    // +0x60 — purpose unknown; type from ~PackageFile

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
static_assert(offsetof(PackageFileView, _string58)   == 0x58,
              "~PackageFile 0x1800b45c0: the String release idiom on [RBX+0x58]");
static_assert(offsetof(PackageFileView, _string60)   == 0x60,
              "~PackageFile 0x1800b4599: the String release idiom on [RBX+0x60]");
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

// ---- Collection::Array<System::Filename>, as FindFiles' out-parameter ------
// DECLARED, NOT DEFINED, and deliberately so. The 16-byte {_data,_size,
// _capacity} header is DEFINED in runtime/src/io/filesystem_find_win.cpp, with
// its own three `static_assert`s pinned to 0x180026880's instructions, and that
// file reaches it through `rtcoll::AsArray<&kSpec_Filename>` — a HAND-DERIVED
// element spec that deliberately does NOT match
// `rtcoll::SpecFor<System::Filename>()` (see the note at `kSpec_Filename`).
// Naming `Collection::Array<System::Filename>` in the declaration below would
// invite exactly the silent substitution that comment forbids, so
// `IO::FileSystem::FindFiles` takes a pointer to this incomplete type instead —
// which is all the declaration needs and all the retail signature pins.
struct FilenameArray;

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

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 4 — the REAL `IO::FileSystem`, PARTIAL
// ============================================================================
//
// Design: B34_RULES.md §1.5 rule 1, §4.3; B34_BRIEFS.md §E.4.
//
// *** IT HAS NO VTABLE. *** There is no `??_7FileSystem@IO@@6B@` in the retail
// export table, no row for it in `standalone/rt_data_mint_tables.inc` and no
// `deleting_dtor.cpp` row; every exported member is `QEAA`/`QEBA`/`AEBA`. So,
// like `IO::Serializer`, `IO::Registry` and `IO::Modification`, converting a
// member here cannot change which table any live object wears.
//
// *** THIS DECLARATION IS DELIBERATELY PARTIAL, AND SO IS THE CONVERSION. ***
// `IO::FileSystem` has nineteen registered members spread over nine files, and
// nine of them sit on the OPEN and READ path that B34_BRIEFS §D binds to
// byte-faithfulness while a separate agent root-causes an intermittent NULL
// resource there. Those bodies are NOT moved in this commit. What IS declared
// below is the four members whose doors live in `runtime/src/io/filesystem.cpp`
// and whose bodies are pure reads — plus the whole LAYOUT, which is the part
// the next owner needs before it can move anything else.
//
// STILL FREE FUNCTIONS, by file, for whoever finishes this class:
//   filesystem_win.cpp            GetSaveDirectory — the one platform helper
//                                 left; it is a `FileSystem` member and it is
//                                 NOT converted, because nothing else in that
//                                 file is and the commit that takes it should
//                                 take the file whole.
// Everything else in `runtime/src/io/filesystem*.cpp` is a member of the class
// below. The NON-`IO::FileSystem` bodies that share those files stay free
// functions on purpose: `RtIo_FileInfo_CopyCtor` / `RtIo_FileInfo_Assign` are
// `FileInfo`'s, and `RtIo_PackageFile_GetFileInfo` / `RtIo_PackageFile_GetFileName`
// are `IO::PackageFile`'s — a POLYMORPHIC class whose conversion is its own
// commit with its own qualified-call rule (B34_BRIEFS §A.1).
//
// *** COMMIT 5a UPDATE (second PACKAGE+FS owner). *** The four NON-§D files are
// converted: `filesystem_setupdirs_win.cpp` (SetupDirectories, GetRootDirectory),
// `filesystem_startup.cpp` (Startup), `filesystem_timestamp_win.cpp`
// (GetFileTimeStamp) and `filesystem_createfile_win.cpp` (CreatePath,
// PlatformCreateFile, CreateFileW, PlatformCloseFile, CloseFile and the
// DESTRUCTOR).
//
// *** COMMIT 5b UPDATE. *** The three §D-BOUND files are converted too, as a
// SEPARATE commit and with their bodies moved VERBATIM — identical control
// flow, identical callees, identical statement order:
// `filesystem_openfile_win.cpp` (OpenFile, ReadFile, PlatformOpenFile),
// `filesystem_packages.cpp` (GetPackDirectory, LoadPackages, ClosePackages) and
// `filesystem_find_win.cpp` (ExistsInPackfile, PlatformExists, FindFiles). No
// lock, no retry, no positioned read and no null check was added there, and
// nothing that makes the path spin was removed: the two-loader race
// (LOADER_READ_SPIN.md) is a known defect with its own pending fix.
// The list above is what is LEFT.
//
// *** AND ONE FACT IN THE COMMIT-4 BANNER WAS WRONG, CORRECTED HERE. *** It said
// `??1FileSystem@IO@@` lives in `iobase_vtables.cpp` and is outside PACKAGE+FS's
// file set. It does not and it is not: `RtIo_FileSystem_Dtor` is, and always was,
// in `filesystem_createfile_win.cpp` (which registers 0x1800ac6d0 there), and
// `iobase_vtables.cpp` contains no occurrence of the string "FileSystem" at all
// (grepped). B34_RULES §6.2 carried the same wrong attribution and is corrected
// in the same commit. The destructor therefore DOES convert, and §4.3's "no real
// constructors" rule is not in tension with that: that rule exists to stop a
// ctor/dtor door swapping the vtable a live object wears, and `IO::FileSystem`
// HAS NO VTABLE — it is the same exemption `IO::Serializer` gets in §4.3's own
// table ("Serializer is NON-polymorphic. It has no vptr, so converting its ctor
// cannot change any table").
//
// THE CONSTRUCTOR (`??0FileSystem@IO@@QEAA@XZ`, 0x1800ac7c0) IS STILL NOT PORTED
// ANYWHERE IN THE TREE and is not invented here. `~FileSystem()` being real does
// not make the class default-constructible into a valid state; every live
// FileSystem is still the one the singleton points at.
//
// *** THE TWO CACHED SINGLETON POINTERS (§E.4) ARE NOT RECONCILED HERE, AND
// *** THAT IS ON PURPOSE. *** `engine/funcs/engine_funcs.h` caches the same
// exported data symbol `?_singleton@FileSystem@IO@@0PEAV12@EA` (0x1801fc6a8)
// TWICE — `g_pFileSystemSingleton` and `g_pFileSystemSingleton2`, the latter
// annotated "separate exref for GetDataDirectory calls". A real
// `FileSystem::Get()` has to reconcile them, and `engine/funcs/` is NOT in this
// block's file set, so no `Get()` is declared: inventing one here would create
// a third spelling of a pointer that already has two. Whoever adds it must
// check BOTH cells, not assume one.
//
// *** THE THREE `RtSystem::String` MEMBERS STAY PODs. *** +0x00, +0x08 and
// +0x10 are `sourceDirectory` / `dataDirectory` / `packDirectory`, and
// `??1FileSystem@IO@@` — whose door is `RtIo_FileSystem_Dtor` in
// `filesystem_createfile_win.cpp`, which registers 0x1800ac6d0 there
// (ENDGAME B.3.4a closing: this line still said "in `iobase_vtables.cpp`,
// outside this file set", which the banner 30 lines above already corrects in
// terms — `iobase_vtables.cpp` contains no occurrence of the string
// "FileSystem" at all) — releases them by hand. Retyping them to `System::String` would add
// an implicit destructor that runs IN ADDITION to that body (the B31
// `RtSystem::String` double-free), and the precondition "the class's own dtor
// body is in your file set" is not met. They are reached through
// `pod_bridge.h`'s `Sys()` / `SysFile()` at each use, exactly as before.
// ============================================================================

namespace IO {

class FileSystem {
public:
    // ?Exists@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z        0x1800ab310
    bool Exists(const RtSystem::Filename& path) const;

    // ?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ 0x180007bd0
    System::Filename GetDataDirectory() const;

    // ?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ 0x1800acfd0
    // ENDGAME B.3.4a closing: the last FileSystem member to move. Unlike the
    // two directory getters above it reads NO field of `this` at all — retail
    // loads the pointer and never dereferences it (filesystem_win.cpp's banner
    // proves that from the disassembly, not the decompile) — so the body is a
    // SHGetFolderPathW call, not a copy ctor. `const` matches retail's `QEBA`;
    // the by-value Filename return is the hidden-retslot ABI, as above.
    System::Filename GetSaveDirectory() const;

    // ?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ 0x180007b90
    // Retail's `MOV RDX,RCX` passes `this` ITSELF to the Filename copy ctor,
    // which is only possible because FileSystem+0x00 IS a String/Filename
    // object. That is the strongest pin this header has on +0x00.
    System::Filename GetSourceDirectory() const;

    // ?GetPackfileId@FileSystem@IO@@QEBAIAEBVFilename@System@@@Z  0x1800ab050
    uint32_t GetPackfileId(const RtSystem::Filename& path) const;

    // ---- commit 5a: the members whose doors live in the four NON-§D files --
    // Each keeps retail's name; the door keeps its own name, its registration
    // lines and its signature (several are called cross-file by
    // file_stream.cpp, text_file_stream_win.cpp, persistence_saveinfo_job.cpp
    // and persistence_worker_win.cpp) and delegates here.

    // -- filesystem_setupdirs_win.cpp ---------------------------------------
    // ?GetRootDirectory@FileSystem@IO@@QEAA?AVFilename@System@@XZ 0x1800ad650
    // NON-const (`QEAA`), and retail never reads `this` — kept as the signature
    // says, exactly as the free function kept its unused `self`.
    System::Filename GetRootDirectory();
    // ?SetupDirectories@FileSystem@IO@@QEAAXXZ                    0x1800abd00
    void SetupDirectories();

    // -- filesystem_startup.cpp ---------------------------------------------
    // ?Startup@FileSystem@IO@@QEAA_NXZ                            0x1800abc50
    bool Startup();

    // -- filesystem_timestamp_win.cpp ---------------------------------------
    // ?GetFileTimeStamp@FileSystem@IO@@QEBA?AVDateTime@System@@AEBVFilename@4@@Z
    //                                                             0x1800ae270
    // Retail returns `System::DateTime` BY VALUE, which is one qword (the raw
    // FILETIME). There is no `System::DateTime` class in this tree yet, so the
    // member returns the qword itself and the door keeps the hidden return slot
    // it has always had. Inventing a DateTime here to make the signature look
    // like the mangling would be a new class with no layout evidence.
    uint64_t GetFileTimeStamp(const RtSystem::Filename& path) const;

    // -- filesystem_createfile_win.cpp --------------------------------------
    // ?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z     0x1800ae430
    bool CreatePath(const RtSystem::Filename& path) const;
    // ?PlatformCreateFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBVFilename@System@@@Z
    //                                                             0x1800ade60
    // ?PlatformCloseFile@FileSystem@IO@@AEAAXAEAUFileInfo@12@@Z    0x1800ae020
    // Both are `AEAA` — PRIVATE in retail, and this DLL exports its private
    // members (reference_aeaa_is_private_not_unexported.md). They are declared
    // PUBLIC here because each has its OWN registered door (0x1800ade60 and
    // 0x1800ae020 are separately registered by name and by address in
    // filesystem_createfile_win.cpp) and a door is a free function, which a
    // private member would not let in. Making the doors `friend`s instead would
    // hide a real fact — that these two are reachable from outside the class in
    // our tree — behind a keyword.
    bool PlatformCreateFile(RtIO::FileInfo& info, const RtSystem::Filename& path);
    void PlatformCloseFile(RtIO::FileInfo& info);
    // ?CreateFileW@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z
    //                                                             0x1800acc50
    // Retail returns `FileInfo` BY VALUE through a hidden return slot, and the
    // member KEEPS THE OUT-PARAMETER rather than returning by value. That is not
    // laziness: `RtIO::FileInfo` is a POD holding two `RtSystem::String` slots,
    // so a by-value return would be a BITWISE copy with no AddRef and no
    // release — the `RtSystem::String` double-free trap from the other side.
    // The slot is raw bytes and the body placement-constructs the two Filenames
    // into it, exactly as retail's 0x1800acc82/8e do.
    RtIO::FileInfo* CreateFileW(RtIO::FileInfo* ret, const RtSystem::Filename& path);
    // ?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z            0x1800accc0
    void CloseFile(RtIO::FileInfo& info);
    // ---- commit 5b: the members whose doors live in the THREE §D-BOUND files
    // B34_BRIEFS §D binds `filesystem_openfile_win.cpp`, `filesystem_packages.cpp`
    // and `filesystem_find_win.cpp` to byte-faithfulness while the two-loader
    // race (LOADER_READ_SPIN.md) has its own pending fix. These nine bodies
    // moved VERBATIM — identical control flow, identical callees, identical
    // statement order. No lock, no retry, no positioned read and no null check
    // was added, and nothing that makes the path spin was removed.

    // -- filesystem_openfile_win.cpp ----------------------------------------
    // ?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z
    //                                                             0x1800ac880
    // Retail returns FileInfo by value; the member keeps the out-parameter for
    // the same reason `CreateFileW` does — see that declaration.
    RtIO::FileInfo* OpenFile(RtIO::FileInfo* ret, const RtSystem::Filename& name);
    // ?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z       0x1800ace00
    // RECURSIVE (0x1800ace7e re-enters itself with the containing package's
    // FileInfo). `ReadFile` is a plain function name in <windows.h>, not a
    // macro, so the member name does not collide; the Win32 call inside the
    // body is spelled `::ReadFile` and always was.
    bool ReadFile(RtIO::FileInfo& fi, int32_t n, void* dst);
    // ?PlatformOpenFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBVFilename@System@@@Z
    //                                                             0x1800adce0
    // `AEAA`, private in retail, public here for the door reason above.
    bool PlatformOpenFile(RtIO::FileInfo& fi, const RtSystem::Filename& name);

    // -- filesystem_packages.cpp --------------------------------------------
    // ?GetPackDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ  0x180007c10
    System::Filename GetPackDirectory() const;
    // ?ClosePackages@FileSystem@IO@@QEAAXXZ                        0x1800abab0
    void ClosePackages();
    // ?LoadPackages@FileSystem@IO@@QEAAXXZ                         0x1800ab460
    void LoadPackages();

    // -- filesystem_find_win.cpp --------------------------------------------
    // ?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z 0x1800ab180
    bool ExistsInPackfile(const RtSystem::Filename& path) const;
    // ?PlatformExists@FileSystem@IO@@AEBA_NAEBVFilename@System@@@Z   0x1800adc70
    // `AEBA`, private in retail; the `QEBA` spelling is not an export at all
    // and would resolve NULL forever. Public here for the door reason above.
    bool PlatformExists(const RtSystem::Filename& path) const;
    // ?FindFiles@FileSystem@IO@@QEBAXAEBVString@System@@0_NAEAV?$Array@VFilename@System@@@Collection@@@Z
    //                                                             0x1800ad7a0
    // The out-parameter is `RtIO::FilenameArray`, declared incomplete above;
    // that comment says why it is not `Collection::Array<System::Filename>`.
    // The two String parameters keep the POD spelling the registered door has,
    // because the door IS this body and the recursion lives in the file-local
    // `FindFilesImpl`, which is where the class spellings already are.
    void FindFiles(const RtSystem::String& dir, const RtSystem::String& pattern,
                   bool recursive, RtIO::FilenameArray* out) const;

    // ??1FileSystem@IO@@QEAA@XZ                                    0x1800ac6d0
    // A REAL destructor — see the banner. It is `QEAA`, non-virtual, and the
    // class has no vtable, so converting it cannot change what any live object
    // wears. There is deliberately NO constructor: 0x1800ac7c0 is unported.
    ~FileSystem();

    // ---- the pinned fields, at retail's offsets ---------------------------
    // PUBLIC for the same reason `IO::Serializer`'s two are: the fifteen
    // unconverted bodies across `runtime/src/io/` and the exe-side probes reach
    // these offsets through `RtIO::FileSystem`, and `offsetof` on them has to be
    // well formed for the pins below. Field-for-field with the POD above, whose
    // per-offset evidence comments are not repeated here.
    RtSystem::String   _sourceDirectory;       // +0x00
    RtSystem::String   _dataDirectory;         // +0x08
    RtSystem::String   _packDirectory;         // +0x10
    uint8_t            _existsInPackfileGate;  // +0x18
    uint8_t            _platformExistsGate;    // +0x19
    uint8_t            _unknown1A[0x20 - 0x1A];
    RtIO::PackageFileView**  _packfiles;       // +0x20
    int32_t            _packfileCount;         // +0x28
    int32_t            _packfileCapacity;      // +0x2c
    RtSystem::String*  _userPackFilenames;     // +0x30
    int32_t            _userPackFilenameCount; // +0x38
    int32_t            _unknown3C;             // +0x3C
    RtIO::PackfileIndexNode* _packfileIndexRoot; // +0x40
    int32_t            _packfileIndexCount;    // +0x48
};

// The POD carries no `sizeof` assert because its tail is opaque padding and
// nothing pins the total size; the class inherits that honestly. A class MAY
// keep opaque padding, but it may never invent a field and may never shrink or
// grow the struct — so every offset below is the POD's, cross-asserted.
static_assert(sizeof(FileSystem) == sizeof(RtIO::FileSystem),
              "IO::FileSystem and the RtIO::FileSystem POD describe the same bytes");
static_assert(offsetof(FileSystem, _sourceDirectory)      == 0x00, "GetSourceDirectory's `(Filename*)this`");
static_assert(offsetof(FileSystem, _dataDirectory)        == 0x08, "GetDataDirectory's `this+8`");
static_assert(offsetof(FileSystem, _packDirectory)        == 0x10, "GetPackDirectory 0x180007c10");
static_assert(offsetof(FileSystem, _existsInPackfileGate) == 0x18, "Exists' `CMP byte [RCX+0x18],0`");
static_assert(offsetof(FileSystem, _platformExistsGate)   == 0x19, "Exists' `CMP byte [RBX+0x19],0`");
static_assert(offsetof(FileSystem, _packfiles)            == 0x20, "GetPackfileId's `*(this+0x20)`");
static_assert(offsetof(FileSystem, _packfileCount)        == 0x28, "the packfile Array's size");
static_assert(offsetof(FileSystem, _packfileCapacity)     == 0x2c, "LoadPackages 0x1800ab5d4/5f0");
static_assert(offsetof(FileSystem, _userPackFilenames)    == 0x30, "the user pack filenames");
static_assert(offsetof(FileSystem, _userPackFilenameCount) == 0x38, "AreUserPackFilesEnabled 0x1800acfc0");
static_assert(offsetof(FileSystem, _packfileIndexRoot)    == 0x40, "GetPackfileId's `*(this+0x40)`");
static_assert(offsetof(FileSystem, _packfileIndexCount)   == 0x48, "ClosePackages 0x1800abbf0");

static_assert(offsetof(FileSystem, _sourceDirectory) == offsetof(RtIO::FileSystem, sourceDirectory), "alias");
static_assert(offsetof(FileSystem, _dataDirectory)   == offsetof(RtIO::FileSystem, dataDirectory),   "alias");
static_assert(offsetof(FileSystem, _packDirectory)   == offsetof(RtIO::FileSystem, packDirectory),   "alias");
static_assert(offsetof(FileSystem, _existsInPackfileGate) == offsetof(RtIO::FileSystem, existsInPackfileGate), "alias");
static_assert(offsetof(FileSystem, _platformExistsGate)   == offsetof(RtIO::FileSystem, platformExistsGate),   "alias");
static_assert(offsetof(FileSystem, _packfiles)       == offsetof(RtIO::FileSystem, packfiles),       "alias");
static_assert(offsetof(FileSystem, _packfileCount)   == offsetof(RtIO::FileSystem, packfileCount),   "alias");
static_assert(offsetof(FileSystem, _packfileIndexRoot) == offsetof(RtIO::FileSystem, packfileIndexRoot), "alias");
static_assert(offsetof(FileSystem, _packfileIndexCount) == offsetof(RtIO::FileSystem, packfileIndexCount), "alias");

static_assert(!std::is_polymorphic<FileSystem>::value,
              "IO::FileSystem must never acquire a vtable: retail has none for it, and a "
              "vptr would push every field eight bytes down under nineteen doors");
static_assert(std::is_standard_layout<FileSystem>::value,
              "the doors reach these fields through a reinterpret_cast");

}  // namespace IO
