# ENDGAME B.3 step 4 — `IO` namespace + `System` rest: design inventory (read-only, 2026-09-18)

Scope: `IO::Stream`/`FileStream`/`BinaryFileStream`/`TextFileStream`/`CompressedStream`
family, `IO::MemoryStream`, `IO::Serializer`/`Encoding`/`ReaderWriter`, `IO::PackageFile`,
`IO::FileSystem`, `IO::Modification`, `IO::Persistence`, `IO::Registry`, plus whatever of
`System` did not convert in B.3.1 (String/Filename). No tree file edited. Format matches
`OBJECT_INVENTORY.md`. All counts below are grep/wc runs against the live tree at
`C:\code\GIT\untitled_rev_eng_project` with `standalone/build*/` and `tests/unit/build/`
excluded (anchored on a path separator); the command is stated next to each number that
isn't a plain file read. Anything not directly read or grepped is marked UNKNOWN.

## 1. File table — `runtime/src/io/*` and `runtime/types/io/*`

`runtime/src/io/*.cpp`: **44 files, 22,220 lines** (`wc -l runtime/src/io/*.cpp` summed).
`runtime/types/io/*.h`: **7 files, 1,813 lines**. Registration totals (`grep -c
"RegisterRuntimeExport"` / `"RegisterRuntimeAddr"` per file, summed): **≈273
RegisterRuntimeExport + ≈320 RegisterRuntimeAddr ≈ 593 rows** — roughly comparable in
scale to `runtime/src/object/*` (388 rows) despite `io/` having more than the double the
line count, because `serialize_family.cpp` alone carries 150 of them (75/75) for the
scalar-serialize dispatch family.

| file | lines | export | addr | retail class(es) / role |
|---|---|---|---|---|
| `binary_writer.cpp` | 261 | 3 | 4 | `IO::BinaryWriter` framing tail (magic/placeholder/version trailer) |
| `closure_batch2b_win.cpp` | 265 | 0 | 2 | cutover closure batch 2b (Win-seam) |
| `compressed_stream_open.cpp` | 196 | 2 | 2 | `IO::CompressedStream::Open` (read-path setup) |
| `compressed_stream_read.cpp` | 487 | 4 | 4 | `IO::CompressedStream` read path under `ReadPackfile` |
| `compressed_stream_release.cpp` | 197 | 2 | 2 | `IO::CompressedStream` window release |
| `compressed_stream_write.cpp` | 779 | 13 | 11 | `IO::CompressedStream` writer half + remaining vtable slots |
| `compressedcopyfilestream.cpp` | 171 | 3 | 3 | `IO::CompressedCopyFileStream`, 4 of 9 vtable slots |
| `compressedstream.cpp` | 847 | 12 | 14 | `IO::CompressedMemoryStream` (scaffolded as `stream.cpp`, renamed — see README) |
| `encoding.cpp` | 397 | 2 | 7 | `IO::Encoding` Binary factory pair + `BinaryReader`/`BinaryWriter` ctors |
| `encoding_visitor_text.cpp` | 135 | 0 | 10 | `IO::Encoding` Visitor (0x1801559f8) + Text (0x1801559d0) tables |
| `file_stream.cpp` | 1345 | 15 | 19 | `IO::FileStream`/`BinaryFileStream`/`TextFileStream` lifecycle + Create |
| `file_stream_win.cpp` | 166 | 2 | 2 | the two `FileStream` vtable slots that touch Win32 |
| `filesystem.cpp` | 372 | 7 | 6 | `IO::FileSystem` + `Modification` (Exists/GetDataDirectory/GetSourceDirectory/GetPackfileId + related) |
| `filesystem_createfile_win.cpp` | 647 | 6 | 6 | `FileSystem::CreatePath/CreateFileW/PlatformCreateFile/CloseFile/PlatformCloseFile/~FileSystem` |
| `filesystem_find_win.cpp` | 742 | 4 | 9 | `FileSystem::Exists`/`FindFiles` |
| `filesystem_openfile_win.cpp` | 569 | 4 | 6 | `FileSystem::OpenFile/ReadFile/PlatformOpenFile/CloseFile` helpers |
| `filesystem_packages.cpp` | 944 | 5 | 6 | `FileSystem::LoadPackages` |
| `filesystem_setupdirs_win.cpp` | 516 | 2 | 2 | `FileSystem::SetupDirectories` |
| `filesystem_startup.cpp` | 152 | 1 | 1 | `FileSystem::Startup` |
| `filesystem_timestamp_win.cpp` | 150 | 1 | 1 | `FileSystem::GetFileTimeStamp` |
| `filesystem_win.cpp` | 190 | 1 | 1 | `FileSystem::GetSaveDirectory` |
| `iobase_vtables.cpp` | 584 | 11 | 15 | `Stream::GetSource` (shared trap), `MemoryStream`/`BinaryReader`/`Visitor`/`VisitorStream` dtors, `Modification::~Modification` |
| `memory_stream.cpp` | 220 | 6 | 6 | `IO::MemoryStream` Write/Read/SetCurrentPos/GetCurrentPos/GetSize/Close |
| `modification.cpp` | 877 | 6 | 22 | `IO::Modification`'s mods-menu cluster + Mod-entry lifecycle |
| `modification_refresh.cpp` | 1250 | 5 | 11 | `Modification::RefreshList` |
| `modification_setupfilesystem.cpp` | 652 | 1 | 1 | `Modification::SetupFileSystem` |
| `modification_sort.cpp` | 131 | 0 | 2 | `Modification::SortMods` |
| `package_file.cpp` | 811 | 24 | 24 | `IO::PackageFile` core |
| `package_file_dtor.cpp` | 316 | 2 | 2 | `~IO::PackageFile` |
| `package_file_read_win.cpp` | 787 | 2 | 1 | `PackageFile::ReadPackfile` |
| `package_file_serialize.cpp` | 331 | 1 | 1 | `PackageFile::Serialize` (vtable slot 5, last unported slot) |
| `persistence.cpp` | 298 | 6 | 5 | `IO::Persistence` core (5 of 7 exports) |
| `persistence_registry_win.cpp` | 1002 | 3 | 3 | `Persistence` registry read half of `Engine::Startup` step 4 |
| `persistence_saveinfo_job.cpp` | 398 | 0 | 2 | `Persistence` worker COMMAND 3 (GET-SAVE-INFO) |
| `persistence_savelist_job.cpp` | 333 | 0 | 1 | `Persistence` worker COMMAND 4 (GET-SAVE-LIST) |
| `persistence_saves.cpp` | 325 | 6 | 6 | `Persistence` save-list bodies |
| `persistence_saves_win.cpp` | 133 | 1 | 2 | `Persistence` save-list Win seam |
| `persistence_startup.cpp` | 151 | 1 | 1 | `Persistence::Startup` |
| `persistence_worker_win.cpp` | 906 | 0 | 7 | `Persistence` worker thread (Win seam) |
| `rt_factories_io.cpp` | 24 | 0 | 0 | `Object::Factory` object-creation slots for `PackageFile` (both delegate to its ctor) |
| `rt_io_port.cpp` | 337 | 1 | 0 | `RtIoPortInit()` — the wave entry point |
| `serialize_family.cpp` | 723 | 75 | 75 | 73 vtable slots across `BinaryReader`/`BinaryWriter`/`Visitor` scalar-serialize dispatch |
| `serializer.cpp` | 393 | 23 | 0 | `IO::Serializer` — all-by-name (see §5) |
| `text_codec.cpp` | 613 | 0 | 5 | `IO::TextCodec::DetectFormat` and related |
| `text_file_stream_win.cpp` | 1097 | 10 | 10 | 5 of 8 unported `TextFileStream` slots |

`runtime/types/io/*.h`: `Registry.h` 88, `Modification.h` 128, `Stream.h` 320,
`FileStream.h` 422, `FileSystem.h` 323, `Persistence.h` 270, `Serializer.h` 262. **No
`PackageFile.h` exists** — `PackageFile`'s own layout lives partly in `FileSystem.h`
(`PackageFileView` :81, `PackageFileEntry` :156) and the rest is undocumented as a single
struct (see §2 surprise). All seven headers are POD structs with `static_assert` pins —
**none of the IO namespace has a real C++ class yet**, unlike `Object::Base`/`BaseTypeInfo`/
`Factory` (B.3.3) or `System::String`/`Filename` (B.3.1).

## 2. Per-class surface

Every class below is a POD struct (`RtIo::X` / `RtIO::X`), reached through hand-built
vtable stamps and `fn_*`/`RT_DATA_SLOT` doors — none is a real class.

### `IO::Stream` (base) — `Stream.h:70-78`, 0x18 bytes
Abstract; `vptr`+`direction`+`encoding`. Vtable `??_7Stream@IO@@6B@` @ 0x1801549a0, 7 slots,
all pinned in `Stream.h:57-68`: `[0]` deleting dtor, `[1]` `GetSource` (real body,
`RtIo_Stream_GetSource`, `iobase_vtables.cpp:347`), `[2..6]` `Write`/`Read`/`SetCurrentPos`/
`GetCurrentPos`/`GetSize` all `_purecall` (0x18007ec28) — pure in the base, overridden per
concrete stream.

### `IO::MemoryStream` — `Stream.h:114-127`, 0x30 bytes
Fields `data`/`ownsData`/`cursor`/`size` at +0x18/+0x20/+0x24/+0x28. Vtable
`??_7MemoryStream@IO@@6B@` @ 0x1801555b0, 8 slots (mint row confirms 8, `rt_data_mint_tables.inc:2882`).
Bodies: `RtIo_MemoryStream_DeletingDtor` (`iobase_vtables.cpp:378`, slot 0, addr-only
0x1800242a0) delegates by EXPORT NAME to `RtIo_MemoryStream_Dtor` (`compressedstream.cpp:657`,
registered both doors at :963-965 — the "missing complement" story, `iobase_vtables.cpp:358-386`).
`memory_stream.cpp` ports the other six virtuals (Write/Read/SetCurrentPos/GetCurrentPos/
GetSize/Close), all six by name+address (`memory_stream.cpp` export=6 addr=6). So
**`IO::MemoryStream` is fully ported at the body level** (8/8 vtable slots have a real
body) despite NEXT_SESSION.md §2a listing it as something the step must land — see §9.3.

### `IO::CompressedStream` — `Stream.h:168-195`, 0x68 bytes
`position`/`activeWindow`/`source`/three `Collection::Array` fields (window/header/block)/
`compression`. Vtable `??_7CompressedStream@IO@@6B@` @ 0x180155640, 9 slots (mint row
confirms 9, `:2013`). Bodies split across `compressed_stream_open/read/release/write.cpp`
and `compressedcopyfilestream.cpp`; deleting dtor is `RT_DELDTOR(0x180023fd0, 0x1800aa650,
0x068, "IO::CompressedStream")` (`deleting_dtor.cpp:427`).

### `IO::CompressedMemoryStream` — `Stream.h:306-313`, 0x98 bytes
`CompressedStream base` + `MemoryStream memoryStream` at +0x68 (a MEMBER, not a base).
Vtable @ 0x180155560, 9 slots (mint row confirms 9, `:1998`); deleting dtor
`RT_DELDTOR(0x1800244a0, 0x1800a9560, 0x098, "IO::CompressedMemoryStream")`
(`deleting_dtor.cpp:305`). Ported in `compressedstream.cpp` (847 lines, the file's whole
purpose per its own header comment).

### `IO::CompressedFileStream` — `FileStream.h:243-250`, 0xb8 bytes
`CompressedStream base` + `BinaryFileStream file` at +0x68. Vtable @ 0x1801555f0, 9 slots
(mint confirms 9, `:1983`); deleting dtor `RT_DELDTOR(0x180024100, 0x1800a9330, 0x0b8,
"IO::CompressedFileStream")` (`deleting_dtor.cpp:428`).

### `IO::CompressedCopyFileStream` — 0xc8 bytes (per dtor row), no dedicated type header entry found beyond the file name
Deleting dtor `RT_DELDTOR(0x1800245f0, 0x1800a92a0, 0x0c8, "IO::CompressedCopyFileStream")`
(`deleting_dtor.cpp:915`). Vtable `??_7CompressedCopyFileStream@IO@@6B@` @ 0x180155510, 9
slots (mint confirms 9, `:1968`). Ported in `compressedcopyfilestream.cpp` (4 of 9 slots;
slot 0's own body lives in `deleting_dtor.cpp` per that file's own header comment).

### `IO::FileStream` — `FileStream.h:134-165`, 0x50 bytes
`filename` (refcounted `String`) + `RtIO::FileInfo file` at +0x20. Vtable
`??_7FileStream@IO@@6B@` @ 0x180154960, 8 slots pinned in the header comment:
`[0]` dtor, `[1]` GetSource, `[2]` Write, `[3]` Read, `[4]` SetCurrentPos,
`[5]` GetCurrentPos, `[6]` GetSize, `[7]` Open (FileStream's own — NOT Close, unlike the
MemoryStream branch). Ported across `file_stream.cpp` (main lifecycle) and
`file_stream_win.cpp` (2 Win32-touching slots).

### `IO::BinaryFileStream` — `FileStream.h:191-195`, 0x50 bytes
`FileStream base` and nothing else — exists only to swap the vtable (only slot 0 differs:
`??_7BinaryFileStream@IO@@6B@` @ 0x1801554d0, 8 slots, mint confirms 8 `:1660`). The
attendance counter `NoteSlotDispatch` (§6) exists specifically because `??_7FileStream@`
and `??_7BinaryFileStream@` share slots 1..7 byte for byte.

### `IO::TextFileStream` — `FileStream.h:329-352`, 0x80 bytes
`FileStream base` + `objectVersion`/`objectName`/`objectType`/`requestedFormat`/
`activeEncoding`/`buffer`/`_unk78`. Vtable `??_7TextFileStream@IO@@6B@` @ 0x180155980, 10
slots (mint confirms 10, `:3993` block above); bounded not by adjacency but by the
UNEXPORTED Text-encoding table it installs at +0x10 (0x1801559d0). Ported in
`file_stream.cpp` (main) + `text_file_stream_win.cpp` (5 of 8 "unported slots" per that
file's own header — arithmetic note: 10-slot table, 2 shared with FileStream (dtor overload,
GetSource), leaves 8 TextFileStream-specific slots, 5 of which this file supplies).

### `IO::VisitorStream` — `FileStream.h:399-403`, 0x18 bytes
`Stream base`, nothing else — a NULL SINK used by `ExternalFactory::Add`'s reference-visit
pass. Vtable @ 0x180155a20, 7 slots (mint confirms 7, `:3870`). All 5 non-dtor bodies plus
ctor/dtor are in `iobase_vtables.cpp` (§3, §6 — this is the file that also documents the
Visitor Encoding at 0x1801559f8).

### `IO::PackageFile` — no dedicated `runtime/types/io/PackageFile.h`
Vtable `??_7PackageFile@IO@@6B@` @ 0x180155700, 9 slots (mint confirms 9, `:2980`).
Deleting dtor `RT_DELDTOR(0x1800259d0, 0x1800b4540, 0x0f0, "IO::PackageFile")`
(`deleting_dtor.cpp:564`, size 0xf0). Bodies: `package_file.cpp` (core, 811 lines, 24/24
registrations — the single biggest by-count file in the directory), `package_file_dtor.cpp`,
`package_file_read_win.cpp` (`ReadPackfile`), `package_file_serialize.cpp` (slot 5,
described in its own header as "THE LAST UNPORTED SLOT OF THAT TABLE" as of Wave 27). Its
own `?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A` mint row exists
(`rt_data_mint_tables.inc:4511`, 0x1801f9ac8) — the only IO class with a `_typeInfo` row
found this pass.

### `IO::FileSystem` — `FileSystem.h`, partial layout (opaque padding beyond touched fields)
Fields modelled: `dataDirectory` (+0x08), two `Exists()` gates (+0x18/+0x19), packfile
array+count (+0x20/+0x28), a second array+count at +0x30/+0x38 (unread), path→packfile BST
root (+0x40, `PackfileIndexNode`, a full AVL node with balance/height at +0x28/+0x2a). No
vtable documented in this header (FileSystem's methods called from this tree are
non-virtual exports). Ported across `filesystem.cpp`,
`filesystem_{createfile,find,openfile,packages,setupdirs,startup,timestamp}_win.cpp`.

### `IO::Modification` — `Modification.h:111-121`, 0x28 bytes
`dirty`/`savingSettings`/`mods`(ptr)/`count`/`capacity`/`savedMods`/`savedCount`/
`savedCapacity`. `Mod` entry struct is 0x80 bytes, fully laid out (`Modification.h:77-99`).
Dtor `RtIo_Modification_Dtor` (`iobase_vtables.cpp:500`) clears the exported singleton
`?_singleton@Modification@IO@@2PEAV12@EA` (tagged `UNPROMOTABLE(HARMFUL)` in
`reg_mint_promoted.cpp` — deliberately left on the DLL) then destroys `savedMods` then
`mods` (that ORDER is retail's and is reproduced, not "tidied"). Ported across
`modification.cpp`, `modification_refresh.cpp`, `modification_setupfilesystem.cpp`,
`modification_sort.cpp`.

### `IO::Persistence` — `Persistence.h`, deliberately partial (no `sizeof` assert)
Only +0x00 (AVL set root of "section+key"→"value"), +0x08 (element count), +0x90
(`_savePath` String) are pinned; +0x0C..+0x8F explicitly UNKNOWN — the header's own banner
forbids extending it from a decompile without a fresh static_assert. Singleton-only: "do not
allocate a Persistence from this declaration". Ported across `persistence.cpp` (5 of 7
exports), `persistence_registry_win.cpp`, `persistence_save{info,list}_job.cpp`,
`persistence_saves*.cpp`, `persistence_startup.cpp`, `persistence_worker_win.cpp`.
`SaveGameInfo` is forward-declared here, defined at the bottom of the header (not read this
pass — UNKNOWN detail).

### `IO::Registry` — `Registry.h:60-65`, 0x18 bytes, COMPLETE (unusually — pinned by the copy ctor enumerating every member)
`path` (String) / `key` (`void*`, HKEY) / `enumIndex` / `enumGroupIndex`. No vtable (not
polymorphic). Ported in `persistence_registry_win.cpp`.

### `IO::Serializer` / `ReaderWriter` / `Encoding` / `BinaryReader` / `BinaryWriter` / `Visitor`
See §5 — these get their own section because they're the design-heaviest part of the step.

## 3. Every vtable

### Hand-built arrays / `Register*Vtable` in `runtime/src/io/`
`iobase_vtables.cpp` is the one file in this directory whose whole purpose is filling in
vtable-slot bodies rather than declaring an array; it does not define a `g_*Vtable[]`
literal — `grep -n "g_.*[Vv]table\[" runtime/src/io/*.cpp` finds none. Instead it stamps
FOUR **exported** vtable pointers by name through `RT_DATA_SLOT`/`ResolveExport`-style doors
(`VtblStream`, `VtblBinaryReader`, `VtblReaderWriter`, `VtblVisitor`, `VtblVisitorStream` —
`iobase_vtables.cpp:287-303`) plus ONE **unexported** one resolved by address
(`VtblVisitorEncoding()`, 0x1801559f8, `:307-312`). This directory's "vtable" work is
therefore minted-table promotion + body-filling, not local array construction — the
opposite pattern from `code/Game`'s `g_*Vtable[]` convention (`OBJECT_INVENTORY.md` §3).

### IO rows in `standalone/rt_data_mint_tables.inc` (`grep -n "@IO@@6B@"`)
19 distinct `??_7...@IO@@6B@` vtable rows (listing below is exhaustive for this pattern; a
few also match `@Audio@@`/`@System@@` because they're bounded by an IO table on the
adjacency side and are included for context):

| class | DLL addr | slots | bound kind |
|---|---|---|---|
| (unexported, Binary Encoding) | 0x1801554a8 | 5 | adjacent-export → `??_7BinaryFileStream@` |
| (unexported, Visitor Encoding) | 0x1801559f8 | 5 | adjacent-export → `??_7VisitorStream@` |
| `??_7BinaryFileStream@IO@@6B@` | 0x1801554d0 | 8 | adjacent-export → `??_7CompressedCopyFileStream@` |
| `??_7BinaryReader@IO@@6B@` | 0x1801552e0 | 26 | adjacent-export → `??_7ReaderWriter@` (see the long correction comment at `:3691` — 26 confirmed 3 ways, not 31 or 36) |
| `??_7BinaryWriter@IO@@6B@` | 0x180155210 | 26 | adjacent-export → `??_7BinaryReader@` |
| `??_7CompressedCopyFileStream@IO@@6B@` | 0x180155510 | 9 | tail-not-a-pointer |
| `??_7CompressedFileStream@IO@@6B@` | 0x1801555f0 | 9 | tail-not-a-pointer |
| `??_7CompressedMemoryStream@IO@@6B@` | 0x180155560 | 9 | tail-not-a-pointer |
| `??_7CompressedStream@IO@@6B@` | 0x180155640 | 9 | tail-not-a-pointer |
| `??_7FileStream@IO@@6B@` | 0x180154960 | 8 | adjacent-export → `??_7Stream@IO@@6B@` |
| `??_7MemoryStream@IO@@6B@` | 0x1801555b0 | 8 | adjacent-export → `??_7CompressedFileStream@` |
| `??_7PackageFile@IO@@6B@` | 0x180155700 | 9 | tail-not-a-pointer |
| `??_7Stream@Audio@@6B@` | 0x180154950 | 2 | adjacent-export → `??_7FileStream@IO@@` (bounds an Audio table, listed because it fences the IO region) |
| `??_7Stream@IO@@6B@` | 0x1801549a0 | 7 | tail-not-a-pointer |
| `??_7TextFileStream@IO@@6B@` | 0x180155980 | 10 | ctor-stamped-neighbour (bounded by an UNEXPORTED Text-encoding table at 0x1801559d0, not by an adjacent export — see the long comment at `:4023` block) |
| `??_7Visitor@IO@@6B@` | 0x1801558b0 | 26 | adjacent-export → `??_7TextFileStream@` |
| `??_7VisitorStream@IO@@6B@` | 0x180155a20 | 7 | tail-not-a-pointer |

None of these rows' `RT_MINT_UNNAMED` slots is a bare guess — each carries a comment
justifying the slot count (adjacency, a ctor's own `LEA`, or a byte-boundary argument).
`RT_MINT_UNNAMED` vs `RT_MINT_NAMED` split not tallied per-row this pass (would need
reading all 19 `kVSlotsNN[]` bodies individually) — UNKNOWN, flagged rather than guessed.

### The two unexported `IO::Encoding` tables (Visitor 0x1801559f8, Text 0x1801559d0)
Both fully read from `encoding_visitor_text.cpp` (135 lines, whole file). Same 5-slot shape
as the shared Binary table (0x1801554a8, ported in `encoding.cpp`): `IsBinary`/`IsText`/
`GetKind` constants, then `CreateReader`(+0x18)/`CreateWriter`(+0x20). Visitor:
`{false,false,3}`, both factory slots byte-identical (`MakeVisitor`, allocates a
`VisitorCodec`, no stream header read/written). Text: `{false,true,1}`, both factory slots
return NULL (`0x1800b9a80`/`0x1800b9a70` are bare `XOR EAX,EAX; RET` — "text serialization
was stripped from the DLL" per the file's own header comment). All 10 bodies registered
by ADDRESS ONLY (`encoding_visitor_text.cpp:121-135`) since neither table is exported.

### IO rows in `deleting_dtor.cpp` (name starts `"IO::`)
8 rows (`grep -n 'RT_DELDTOR.*"IO::' runtime/src/object/deleting_dtor.cpp`):

| addr | dtorVa | size | name |
|---|---|---|---|
| 0x18000c610 | 0x1800b9a40 | 0x018 | `IO::Stream` |
| 0x1800244a0 | 0x1800a9560 | 0x098 | `IO::CompressedMemoryStream` |
| 0x180023fd0 | 0x1800aa650 | 0x068 | `IO::CompressedStream` |
| 0x180024100 | 0x1800a9330 | 0x0b8 | `IO::CompressedFileStream` |
| 0x18002ba90 | 0x1800ba490 | 0x080 | `IO::TextFileStream` |
| 0x180023bd0 | 0x1800b9150 | 0x020 | `IO::ReaderWriter` |
| 0x1800259d0 | 0x1800b4540 | 0x0f0 | `IO::PackageFile` |
| 0x1800245f0 | 0x1800a92a0 | 0x0c8 | `IO::CompressedCopyFileStream` |

Note: `IO::MemoryStream`, `IO::FileStream`, `IO::BinaryFileStream`, `IO::VisitorStream`,
`IO::Visitor`, `IO::BinaryReader`, `IO::Modification` do NOT have `RT_DELDTOR` rows — their
deleting dtors are ported directly as free functions in `iobase_vtables.cpp` (their vtable
slot 0 is filled by a `RegisterRuntimeAddr` on the deleting-dtor thunk address itself, not
through the `RT_DELDTOR` table). Two different mechanisms coexist in this namespace already,
mirroring the Object family's `deleting_dtor.cpp` vs `iobase_vtables.cpp`-style direct-port
split — worth stating explicitly for whoever designs the eventual unification (§ surprises).

### IO `?_typeInfo@`/storage rows
Exactly **one**: `?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A` at 0x1801f9ac8
(`rt_data_mint_tables.inc:4511`). No other IO class carries a reflection record in the mint
tables (`grep -n "_typeInfo@.*@IO@@" standalone/rt_data_mint_tables.inc` → 1 hit).

## 4. `SerializerLite`

`RtObject::SerializerLite` (`runtime/types/object/Internal.h:212-216`): a 2-field, 0x10-byte
POD — `{ void* stream; void* readerWriter; }` — a deliberate stand-in for `IO::Serializer`
(the real 0x10-byte layout in `runtime/types/io/Serializer.h:253-259` is IDENTICAL in shape:
`{Stream* stream; ReaderWriter* readerWriter;}` — SerializerLite is not a guess, it is the
same struct redeclared in the Object namespace so `runtime/src/object/*` doesn't have to
depend on `runtime/types/io/`). Built-on constants, same region (`Internal.h:227-231`):
`kEncSerializeBool = 0x78`, `kEncSerializeUint = 0x98`, `kEncSerializeInt = 0xa0` — these are
`ReaderWriter` vtable byte offsets (matching `rw_vtbl::kScalarBool/kScalarUInt/kScalarInt` in
`Serializer.h:203,207,208` exactly), the retail (not profile) numbers.

**Users of `RtObject::SerializerLite` by directory** (`grep -rn "SerializerLite"`,
comments/strings not stripped separately — all hits below are real code or the file-header
comments describing it, none in `code/Game`, `standalone` or `engine`):

- `runtime/types/object/Internal.h` — 6 occurrences (definition + 4 method signatures using
  it: `InternalPtr<T>::Serialize`, `Internal::Serialize`, `Internal::SerializeArray`, plus
  the ctor comment).
- `runtime/src/object/internal.cpp` — 2 (the free function
  `RtObject_InternalBasePtr_Serialize` and a comment reference).
- `runtime/src/object/internal_class.cpp` — 2 (a local `RtObject::SerializerLite ser;` +
  a comment).
- `runtime/src/object/internal_ptr_array_serialize.cpp` — 3 (`using` + 2 signatures).
- `runtime/src/object/external_class_doors.cpp` — 2, comments only, describing the
  policy ("stays on `RtObject::SerializerLite`... until then").

**Total: 15 hits, ALL in `runtime/src/object/` and `runtime/types/object/`** — zero in
`code/Game/`, `standalone/`, or `engine/`. So `SerializerLite`'s blast radius for this step
is confined to the Object family's Serialize bodies, not spread across the game logic —
good news for scoping B.3.4 step 4's coexistence proof.

**Other distinct idioms for "call an Encoding vtable slot N through a Serializer"** —
searched for `SerLite`, `EncVt`, `ENC_SLOT` (zero hits, tree-wide — these spellings do not
exist in this codebase; ruled out, not merely unfound), and for vtable-indexing on a
variable literally named `enc`/`encoding`/`ser` (`grep -rn "enc)\[\|encoding)\[\|ser)\["` —
zero hits). Three REAL idioms found instead:

1. **`RtObject::SerializerLite` + `kEncSerialize*` constants** (above) — 15 users, all in
   `runtime/src/object/`.
2. **`rt::io::rw_vtbl::kXxx` named constants over a real `ReaderWriter*`** — defined in
   `runtime/types/io/Serializer.h:187-214` (26 named slot constants, `kDtor`..
   `kSerializeBytes`). Used via `grep -rl "rw_vtbl::"` in **50 files**, overwhelmingly
   outside `runtime/src/io/` itself: `runtime/src/achievement/` (3), `runtime/src/audio/`
   (3), `runtime/src/collision2d/` (2), `runtime/src/graphics/` (7), `runtime/src/object/`
   (2: `external_ptr_serialize.cpp`, `external_reftable.cpp`), `runtime/src/pathfinding/`
   (1), `runtime/src/picking/` (1), `runtime/src/save/` (2), `runtime/src/ui/` (21),
   `runtime/src/video/` (5), plus `runtime/types/ui/Dialog.h`. **This is the dominant
   idiom by user count** — every class's own `Serialize` body that reaches a
   `ReaderWriter` (not a `SerializerLite`) uses these named constants, not a bare hex
   literal.
3. **Raw hex offsets inline on a variable named `enc`/`stream`/`ser`, no named constant** —
   found in `runtime/src/object/external_reftable.cpp` (`stream->encoding` `+0x10`,
   `stream->direction` `+0x08`, both spelled as field reads on the real `rt::io::Stream`
   type, not as hex — see §5/§9.2) and `runtime/src/io/serializer.cpp`'s own `RwSlot()`
   helper, which takes a raw `int byteOffset` and is called with the `rw_vtbl::` constants
   from idiom 2 — i.e. idiom 3 is really idiom 2's implementation, not a fourth spelling.

**So there are effectively TWO distinct spellings** of "reach an Encoding/ReaderWriter slot
through a Serializer", not the many the design question worried about: the Object-family
`SerializerLite`+`kEncSerialize*` pair (15 users, confined to `runtime/src/object/`), and the
IO-family `rw_vtbl::` named-constant table (50 users, spread through nearly every
`runtime/src/*` directory that ports a `Serialize` method). A real `IO::Serializer`/
`ReaderWriter` class needs to satisfy both call shapes during coexistence, but there are
only two, not an open-ended set.

## 5. `IO::Serializer` and `Encoding` as ported today

### Struct layouts (`runtime/types/io/Serializer.h`)
- `Encoding` (`:97-99`) — `{ void** vtable; }`, 8 bytes, no other fields (a pure policy
  tag; `this` is never read by either factory, per `encoding.cpp`'s own note).
- `ReaderWriter` (`:118-124`) — 0x20 bytes: `vtable`+`allowCustomSerialize`(+0x08, byte)+
  `serializer`(+0x10, `void*` back-pointer)+`stream`(+0x18, `== serializer->stream`).
  3 static_asserts (+0x08/+0x10/+0x18).
- `BinaryCodec` (`:136-144`) — `ReaderWriter base` + `word20`+`word24`+`version`(+0x28)+
  pad; 0x30 bytes (matches the allocator's `MemAlloc(0x30,4)` request). 2 static_asserts.
- `VisitorCodec` (`:165-171`) — `ReaderWriter base` + `version`(+0x20)+pad; 0x28 bytes
  (matches `MemAlloc(0x28,4)`). 2 static_asserts.
- `Stream` (`:234-239`, the Serializer-lane's OWN copy, separate from `RtIo::Stream` in
  `Stream.h` — same 3 fields, same offsets, duplicated across the two lane headers rather
  than shared) — `vtable`+`direction`(+0x08)+`encoding`(+0x10). 2 static_asserts.
- `Serializer` (`:253-259`) — `{ Stream* stream; ReaderWriter* readerWriter; }`, 0x10
  bytes. 3 static_asserts (2 offsets + sizeof) — this is the struct `SerializerLite` mirrors
  (§4).

**Duplicate PODs, confirmed**: `rt::io::Stream` (`Serializer.h:234`) and `RtIo::Stream`
(`Stream.h:70`) are two independently-declared 0x18-byte structs with the same three fields
at the same offsets, in two different namespaces (`rt::io` vs `RtIo`) — the same
duplication pattern OBJECT_INVENTORY found NOT present for Object (§1 there) but which
DOES exist here across the Wave 4 lane-A/lane-B boundary. Neither header includes the
other. A real `IO::Stream` class collapses these two into one, same as `BaseTypeInfo`
collapsed two structs in B.3.3 §2.

### Encoding slot table (documented in `Serializer.h:97-106` and cross-checked in `serializer.cpp:36-93`)
| slot (byte) | meaning | retail evidence |
|---|---|---|
| +0x18 | `CreateReader(Serializer*, int version) -> ReaderWriter*` | `Serializer` ctor's `CALL [RAX+0x18]` when `stream->direction==0` |
| +0x20 | `CreateWriter(Serializer*, int version) -> ReaderWriter*` | `Serializer` ctor's `CALL [RAX+0x20]` |

### `ReaderWriter` vtable (`rw_vtbl` namespace, `Serializer.h:187-214`) — 26 named slots
`kDtor`(0x00), `kGetVersion`(0x08), `kSkip`(0x10), 8 array-Serialize slots (0x18-0x68 by
name/type: char/bool/float/uint64/int64/uint/int/ushort/short/uchar/schar — the header
actually lists both array AND scalar forms in this range, 8 array + then), 12 scalar-
Serialize slots (0x70-0xC0), `kSerializeBytes`(0xC8). Doubly pinned: (a) each `Serializer`
forwarder's own tail-jump displacement (b) the contents of `??_7BinaryReader@IO@@6B@`
(0x1801552e0) and `??_7BinaryWriter@IO@@6B@` (0x180155210) themselves. Explicit warning in
the header (`:183-186`): **the profile build renumbers three of these slots** (bool→0x90,
uint→0xb0, int→0xb8) — never cross-check against a profile decompile.

### Per-Encoding-subclass slots documented
- **Binary** (0x1801554a8, unexported, shared table) — 5 slots: IsBinary/IsText/GetKind
  constants + CreateReader/CreateWriter → `BinaryReader`/`BinaryWriter` ctors
  (`0x1800a8340`/`0x1800a8a30`), each consuming/emitting a 12-byte stream header (magic +
  placeholder + version) — ported in `encoding.cpp`.
- **Visitor** (0x1801559f8, unexported) — 5 slots, `{false,false,3}` + both factories →
  `MakeVisitor` (byte-identical bodies) — ported in `encoding_visitor_text.cpp`.
- **Text** (0x1801559d0, unexported) — 5 slots, `{false,true,1}` + both factories return
  NULL (text serialization stripped from retail) — ported in `encoding_visitor_text.cpp`.

### `serializer.cpp` — 22 of the 22 `IO::Serializer` exports our tree calls, ALL by name
(`export=23, addr=0` — the 23rd is the file's own `RtIoSerializerRegister` entry point, not
a member). No address-door registration exists for this class at all; every one of the 21
`Serialize(...)` overloads plus ctor/dtor/`GetVersion`/`GetEncoding` is reached purely by
mangled name (`serializer.cpp:361-388`). 4 members are explicitly NOT ported (`Skip`,
`GetStream`, `GetDirection`, `Serialize(char&)`) because "our tree does not call them" per
the file's own header — an uncalled port is an unverified port, the same rule B.3.3 states
for `GetTypeIdByName`.

## 6. vptr-identity reads and minted-vtable stamps in `runtime/src/io/`

`iobase_vtables.cpp` and `file_stream.cpp`/`file_stream_win.cpp` are the only files in this
directory with vtable-identity code (no `*(void**)` comparison/assignment or `??_7` symbol
resolution found in the other 41 files this pass).

- **`iobase_vtables.cpp`** stamps five exported and one unexported vtable through
  `RT_DATA_SLOT`/`ResolveAddr` doors (§3), and does the standard base→derived→base RESTAMP
  pattern in every dtor it defines (`RtIo_BinaryReader_Dtor`, `RtIo_Visitor_Dtor`,
  `RtIo_VisitorStream_Dtor`, `RtIo_VisitorStream_Ctor` — each writes `self->base.vptr`
  twice, once to the derived table then back to the base, or vice versa, mirroring
  `goal.cpp`'s `BaseVtable()`/`ExternalVtable()` stamping pattern OBJECT_INVENTORY §risk 4
  describes for the Object family). These are STAMPS, not identity comparisons — same
  classification as `goal.cpp`, not a "which class is this" read.

- **`file_stream.cpp:390-395` `NoteSlotDispatch(const void* vptr)`** — the ONE true
  identity comparison in this directory:
  ```
  void NoteSlotDispatch(const void* vptr) {
      ++g_bfsp.slotEntries;
      const void* ours = OurBfsTable();     // RT_DATA_SLOT("??_7BinaryFileStream@IO@@6B@")
      if (vptr == ours) ++g_bfsp.slotViaMintBfs;
      else              ++g_bfsp.slotViaOther;
  }
  ```
  It is an **attendance counter**, not a behavioural branch: it answers "did this call
  arrive through OUR minted `BinaryFileStream` table?" for a `BfsPromoteCounters` struct
  (`file_stream.cpp:367-371`) that exists BECAUSE `??_7FileStream@` and
  `??_7BinaryFileStream@` share slots 1..7 byte-for-byte (`:358-362`), so a plain per-body
  hit counter cannot tell which of the two promoted rows actually dispatched. **Callers**
  (`grep -rn "NoteSlotDispatch"`, 5 real call sites + 3 doc references):
  `file_stream_win.cpp:100`, `file_stream_win.cpp:135`, `file_stream.cpp:1042`,
  `file_stream.cpp:1074`, `file_stream.cpp:1098`, `file_stream.cpp:1104`,
  `file_stream.cpp:1138`. B33_RULES §5 and NEXT_SESSION §2a both already flag this: it
  "dies with `IO::BinaryFileStream`'s class" and must not be left silently counting zero —
  confirmed still present and still live at this pass.

- No other `runtime/src/io/*.cpp` file resolves a `??_7` symbol for an identity compare —
  the remaining ~15 `??_7` hits tree-wide in this directory (from `grep -n '??_7'
  runtime/src/io/*.cpp`) are all either the RT_DATA_SLOT stamp calls already covered above,
  or comments.

## 7. setjmp/longjmp/SEH — whole-tree sweep

**Surprising result: there is no live `setjmp`, `longjmp`, `__builtin_setjmp`, `__try`, or
C++ `throw`/`try` anywhere in `code/`, `runtime/`, `standalone/`, `engine/`, or `dx11/`.**

- `setjmp`/`longjmp`: 2 hits tree-wide, BOTH inside comments referencing a memory-file name
  (`reference_longjmp_unwind_runs_retail_cleanup.md`) — `runtime/src/achievement/
  goal_serialize.cpp:91` and `runtime/src/input/keymap_load.cpp:79`. No executable
  setjmp/longjmp call exists in the tree today.
- `__builtin_setjmp`: 0 hits.
- `__try`/`__except`: 5 hits, all in comments explaining that MinGW/GCC on x64 has no
  usable `__try`/`__except` and so retail's SEH could not be reproduced — `code/Game/
  Dialog/residenceui.cpp:38`, `code/Game/Dialog/storageui.cpp:618`, `runtime/src/video/
  enumerate_video_modes.cpp:57,70`, `standalone/main.cpp:464`.
- `RaiseException`/`AddVectoredExceptionHandler`: 1 real call —
  `standalone/main.cpp:1329` `AddVectoredExceptionHandler(1, FirstChanceFailFast)`, the
  process-wide crash-log installer (`Init()`, ~line 1325) — infrastructure, not IO/save
  related.
- C++ `throw`/`try`: 0 real occurrences. The 8 hits for `\bthrow\b` are all comments using
  the English word "throw away" (`growthcomponent_serialize.cpp:136`, `sunlight.cpp:386`,
  `modification_refresh.cpp:158`, `documenttext.cpp:709`, `userinterface.cpp:1354`,
  `External.h:562`, `SpriteCapture.h:47`, `dx11/src/buffers.cpp:66`) — none is C++
  exception syntax, and `grep -rn "\btry\s*{"` returns 0 hits tree-wide.

**This directly contradicts NEXT_SESSION.md §2a's "~25 files use setjmp/longjmp or SEH to
cross retail frames (ENDGAME decision 7)"** — see §"surprises" below.

## 8. `System` — the rest of the namespace

`runtime/src/system/*` (15 files) and `runtime/types/system/*` (5 headers):

| header | class(es) | status |
|---|---|---|
| `String.h` | `String` (`:138`, real `class`) vs `RtSystem::String` POD (`:81`) | **REAL CLASS since B.3.1** |
| `Filename.h` | `Filename` (`:72`, real `class`) vs `RtSystem::Filename` POD (`:44`) | **REAL CLASS since B.3.1** |
| `Thread.h` | `CriticalSection` (`:31`, `struct`), `Thread` (`:69`, `struct`) | POD |
| `MemoryAllocator.h` | `MemoryTag`/`FreeTag`/`MemoryRange`/`MemoryAllocator`/`RtSysMemPlatform` (all `struct`) | POD |
| `Value.h` | `Random` (`:48`, `struct`), `Color` (`:90`, `struct`) — no separate `Value` type found in this header despite the filename | POD |

No `Registry.h`/`Crc.h`/`Timer.h` type header exists under `runtime/types/system/` — `Crc`
and `Timer` are implemented directly in `.cpp` (`crc.cpp` 71 lines, `timer.cpp` 343 lines +
`timer.h` alongside it in `runtime/src/system/`, not `runtime/types/system/`) with no
externally-visible struct to convert; `IO::Registry` (§2) lives in the IO namespace instead
of System despite the name-adjacency.

Per-file `.cpp` sizes and registration counts (`runtime/src/system/*.cpp`, `grep -c
"RegisterRuntimeExport"`/`"RegisterRuntimeAddr"`):

| file | lines | export | addr |
|---|---|---|---|
| `allocator.cpp` | 1503 | 22 | 5 |
| `allocator_win.cpp` | 136 | 6 | 0 |
| `closure_batch2b.cpp` | 215 | 3 | 6 |
| `compression.cpp` | 631 | 4 | 2 |
| `configuration.cpp` | 740 | 5 | 6 |
| `crc.cpp` | 71 | 1 | 1 |
| `error.cpp` | 579 | 3 | 3 |
| `filename.cpp` | 574 | 9 | 9 |
| `rt_system_port.cpp` | 64 | 0 | 0 |
| `setupdir_helpers.cpp` | 146 | 3 | 3 |
| `string.cpp` | 1349 | 16 | 16 |
| `thread_win.cpp` | 331 | 10 | 1 |
| `timer.cpp` | 343 | 10 | 0 |
| `value_types.cpp` | 530 | 20 | 0 |
| `widestring.cpp` | 599 | 9 | 8 |

**Caller counts of `fn_<Class>_*` in `engine/funcs/engine_funcs.h`** — attempted with
`grep -c "fn_Thread_\|fn_CriticalSection_\|fn_MemoryAllocator_\|fn_Registry_\|fn_Value_\|
fn_Crc_\|fn_Timer_\|fn_Random_\|fn_Color_\|fn_Compression_\|fn_Configuration_\|fn_Error_\|
fn_WideString_"` per class: only **`Random` (16)** and **`Configuration` (6)** produced any
hits; every other class returned 0. This is almost certainly a NAMING MISMATCH, not an
absence of callers — `MemoryAllocator`'s exports alone total 27 (22+5) in `allocator.cpp`
and must be called from somewhere, so the `fn_MemoryAllocator_*` spelling this pass guessed
is probably wrong (e.g. it may be `fn_Mem*` or resolved through a different macro family
entirely, as `System::Thread`'s ops might be under `fn_Thread*` without the trailing
underscore, or reached only via IAT-cached IO-side wrappers as B.3.3 found for
`InternalBasePtr`). **Flagged as UNKNOWN / needs a follow-up grep with the real spellings**,
not asserted as "these classes have few callers".

## 9. Unported/pending items named in NEXT_SESSION.md §2a

### 9.1 `Internal::Clone` (0x1800bfb10)
Confirmed **UNPORTED**, exactly as NEXT_SESSION states. The only tree reference is
`runtime/src/object/internal_class.cpp:30-37`'s own header comment: `?Clone@Internal@Object@@
QEAAPEAV12@XZ` (0x1800bfb10) — "nothing in the tree registers it by name or by address... a
serialize round-trip deep copy (a 0x1000-byte MemoryStream pair, the Factory descent,
`IsDerivedFrom`, then slot 3 both ways)... porting it needs `IO::MemoryStream`... NOT
declared on the class, because a declared method with no body is a link error waiting for
its first caller." No stub, no TODO macro, no registration — literally absent. Given §2's
finding that `IO::MemoryStream` is ALREADY fully ported at the body level (8/8 vtable slots
have real bodies), **`Internal::Clone`'s stated blocker (needs `IO::MemoryStream`) is
already satisfied** — it may be portable now rather than only after this step, though the
Factory descent + `IsDerivedFrom` dependencies were not re-verified this pass.

### 9.2 `ExternalFactory::SerializeReferenceTable`
**Fully ported already**, as a free function: `RtObject_ExternalFactory_SerializeReferenceTable
(ExternalFactory* self, Stream* stream)` (`external_reftable.cpp:192-193`, 292 retail
instructions at 0x1800bcb60, registered by name+address at `:438-441`). The mangled name is
`?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` — its second
parameter is `IO::Stream&`, the REAL `rt::io::Stream` type from `runtime/types/io/
Serializer.h`, not a file-local struct. What the function DOES build locally is a
`Serializer ser;` on the stack (`external_reftable.cpp:208-210`, the real 16-byte
`rt::io::Serializer` shape) as an adapter, "because passing a Serializer straight through
would read 16 bytes past the end of a 16-byte object" (`:29-37`) — the file's own header
explains that the mangled name's `Stream&` and the locally-built `Serializer` are
deliberately different objects. **NEXT_SESSION's description ("2nd parameter is a
file-local struct today") does not match what this pass found** — the 2nd parameter is the
real `IO::Stream` type; if a file-local struct is meant, it may refer to `self`'s type
(`ExternalFactory*`, declared in `runtime/types/object/External.h`, not file-local either)
or to something this pass didn't locate. Flagged as a discrepancy for the step owner to
resolve, not resolved here.

### 9.3 `IO::MemoryStream`'s port state
**Fully ported at the body level**, contradicting the implication that it's a blocking
prerequisite still to be done. Evidence (§2 above): `iobase_vtables.cpp` ports the deleting
dtor (delegating to `compressedstream.cpp:657`'s `RtIo_MemoryStream_Dtor`, already
registered since Wave 9); `memory_stream.cpp` (220 lines) ports the other six virtuals
(Write/Read/SetCurrentPos/GetCurrentPos/GetSize/Close), all six registered by both name and
address. `compressedstream.cpp` (847 lines, 132 `MemoryStream` mentions) additionally
embeds a `MemoryStream` member inside `CompressedMemoryStream` and drives it. No TODO/stub
marker found anywhere referencing `MemoryStream` as incomplete. What remains is turning the
POD into a real class — the same B.3-step-4 task as every other IO class, not a missing
port.

## Surprises

1. **NEXT_SESSION.md §2a's setjmp/longjmp/SEH claim ("~25 files... ENDGAME decision 7") does
   not hold.** The tree-wide sweep (§7) found ZERO live setjmp/longjmp/`__try`/`__except`/
   C++ throw sites — every hit is a comment. Either the ~25 number refers to a different,
   now-resolved state of the tree, or it was never re-measured after MinGW's lack of SEH
   support forced those constructs out in favour of the `[GUL::Load]`/`FirstChanceFailFast`-
   style VEH crash handling this project actually uses. Whoever writes `B34_RULES.md` §2b.4
   ("the exception/longjmp sites in IO: which become C++ exceptions, which stay, and why")
   should know the answer is "none exist to convert" before scoping that question.
2. **`IO::MemoryStream` is not a blocker** (§9.3) — it's already fully ported at the body
   level, and `Internal::Clone`'s only stated blocker was `IO::MemoryStream`, so
   `Internal::Clone` may be portable as a small side task rather than something that must
   wait for this whole step.
3. **`ExternalFactory::SerializeReferenceTable`'s "file-local struct" description doesn't
   match the code** (§9.2) — its 2nd parameter is the real `IO::Stream` type. Worth a
   5-minute conversation with whoever wrote that NEXT_SESSION line before the step starts,
   in case it refers to something this pass didn't find.
4. **Two duplicate `Stream` PODs already exist** across the Wave-4 lane-A/lane-B boundary
   (`rt::io::Stream` in `Serializer.h` vs `RtIo::Stream` in `Stream.h`, §5) — the same kind
   of duplication OBJECT_INVENTORY explicitly found ABSENT for the Object family. A real
   `IO::Stream` class collapses these for free, but whoever writes it should grep both
   names before assuming there's only one legacy POD to retire.
5. **The IO namespace already has two independent "port a vtable slot 0" mechanisms
   coexisting** (§3): the `RT_DELDTOR` table (8 IO rows) for some classes, and direct
   `RegisterRuntimeAddr` on the deleting-dtor-thunk address in `iobase_vtables.cpp` for
   others (`MemoryStream`, `FileStream`, `BinaryFileStream`, `VisitorStream`, `Visitor`,
   `BinaryReader`, `Modification`). A real-class design needs to know which mechanism each
   class's slot 0 currently uses before retiring anything — this mirrors, but is not
   identical to, B.3.3's `deleting_dtor.cpp` vs `factory_dtors.cpp` split (OBJECT_INVENTORY
   §risk, corrected item 4 in B33_RULES §9 step 7).
6. **`IO::PackageFile` is the only IO class with a `?_typeInfo@` mint row** — every other
   IO class has no reflection record at all in the mint tables, unlike Object's five
   (`Base`/`External`/`ExternalList`/`CompiledExternal`/`Internal`). Whether `PackageFile`
   even needs `DECLARE_TYPE`/`DEFINE_TYPE` treatment when it converts, or whether that one
   row is inert/orphaned, was not determined this pass — flag for the step-4 owner.
7. **No `runtime/types/io/PackageFile.h` exists** despite `PackageFile` being the
   single largest-by-registration-count file in the directory (24/24 in `package_file.cpp`
   alone) — its layout is scattered across `FileSystem.h`'s `PackageFileView`/
   `PackageFileEntry` and whatever `package_file.cpp`/`package_file_dtor.cpp` assert
   locally (not read exhaustively this pass — a full struct layout for `PackageFile` itself
   was not located and should be treated as UNKNOWN until the step owner reads
   `package_file.cpp`'s own static_asserts in full).
8. **The `System` rest's `fn_*` caller-count grep (§8) came back nearly empty** for 11 of
   13 classes — almost certainly because the real `engine_funcs.h` naming for
   `Thread`/`CriticalSection`/`MemoryAllocator`/`Crc`/`WideString`/etc. doesn't match the
   `fn_<Class>_*` pattern this pass guessed. A caller-count table for `System` rest needs a
   fresh pass reading actual `engine_funcs.h` symbol names before B34_RULES can size a
   caller batch for it.

Every number above was produced by a grep/wc run against the live tree in this session
(`C:\code\GIT\untitled_rev_eng_project`, `standalone/build*/` and `tests/unit/build/`
excluded throughout); the pattern is stated next to each count. Anything without a
citation is explicitly marked UNKNOWN rather than guessed.
