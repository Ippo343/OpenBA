#pragma once
// ============================================================================
// runtime/types/io/FileStream.h — IO::FileStream / BinaryFileStream /
//        CompressedFileStream / TextFileStream / VisitorStream
//        (Wave 11 follow-up 2, lane IOSTREAM)
// ============================================================================
//
// runtime/types/io/Stream.h pins the four MEMORY-backed streams. This header
// pins the five that were missing, and they are the ones three Object rows
// stack-construct by value:
//
//     ?Load@External@Object@@         CompressedFileStream over BinaryFileStream
//     ?SaveText@External@Object@@     TextFileStream
//     ?Add@ExternalFactory@Object@@   VisitorStream
//
// ---------------------------------------------------------------------------
// WHY THIS HEADER IS THE WHOLE BLOCKER, AND WHY IT IS NOT "no constructor"
// ---------------------------------------------------------------------------
// Lane OBJEXT recorded these three rows as blocked because the classes have
// "no layout OR a constructor in the tree". Only the first half is true, and
// the distinction decides how big the work is.
//
// EVERY constructor and destructor in this family is EXPORTED:
//
//     ??0FileStream@IO@@QEAA@XZ                              ordinal  188
//     ??0BinaryFileStream@IO@@QEAA@XZ                         ordinal   55
//     ??0CompressedFileStream@IO@@QEAA@XZ                     ordinal  104
//     ??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z      ordinal  441
//     ??0VisitorStream@IO@@QEAA@XZ                            ordinal  511
//     ??1FileStream / ??1BinaryFileStream / ??1CompressedFileStream /
//     ??1TextFileStream / ??1VisitorStream                    all UEAA, all exported
//
// (Run against the export table, not grepped. `AEAA`/`IEAA` mean PRIVATE and
// PROTECTED, not unexported — this DLL exports both, and
// reference_aeaa_is_private_not_unexported.md is the lane that learned it.)
//
// So a caller can construct any of these by calling retail's own ctor on a
// stack buffer. What it CANNOT do without this header is size that buffer or
// reach the fields the caller itself reads — and all three callers read fields
// directly, because MSVC inlined each destructor into the caller's epilogue.
//
// The technique is not new here either: runtime/src/save/saveload_meta.cpp
// already stack-constructs an IO::CompressedMemoryStream this way, piecemeal
// through exported ctors with the vtable stamped by RT_VTABLE_ONCE.
//
// ---------------------------------------------------------------------------
// EVERY SIZE BELOW IS MEASURED, NOT INFERRED
// ---------------------------------------------------------------------------
// Each class's deleting-destructor thunk (vtable slot 0) calls
// `_eh_vector_destructor_iterator_(p, STRIDE, count, ~Class)`, and that stride
// is the element size an array-delete uses, i.e. sizeof. This is the same
// method Stream.h used to pin sizeof(CompressedMemoryStream) == 0x98.
//
//     FileStream / BinaryFileStream   0x180023d80   stride 0x50
//     CompressedFileStream            0x180024100   stride 0xb8
//     TextFileStream                  0x18002ba90   stride 0x80
//     VisitorStream                   0x18002bf80   stride 0x18
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "Stream.h"      // RtIo::Stream, RtIo::CompressedStream, IO::Stream
#include "FileSystem.h"  // RtIO::FileInfo   (note the capital O — a different
                         // namespace spelling, and both are long-standing)
#include "../system/Filename.h"   // System::Filename — FileStream slot 7's argument

namespace RtIo {

// ============================================================
// IO::FileStream — 0x50 bytes. A Stream over a FileSystem file.
//
//   +0x00..0x17  Stream
//   +0x18  String   _filename   the path Create/Open last bound, refcounted
//   +0x20  FileInfo _file       0x30 bytes — RtIO::FileInfo, ALREADY PINNED
//
// THE +0x20 IDENTIFICATION IS THE LOAD-BEARING CLAIM IN THIS HEADER, and it
// collapses most of the layout work, so it is pinned four independent ways
// rather than asserted:
//
//  1. ??1FileStream@IO@@UEAA@XZ (0x1800aaab0) does
//         if (*(int*)(this+0x20) != -1) FileSystem::CloseFile(_singleton, this+0x20);
//         FUN_18000c180(this + 0x20);
//     — it passes `this+0x20` to CloseFile AS a FileInfo*, and FUN_18000c180
//     is ~FileInfo: its whole body releases two refcounted strings at +0x28 and
//     +0x20 of its argument, which are exactly FileInfo::_name1 and _name0.
//
//  2. ?Write@FileStream@IO@@UEAA_NHPEBX@Z (0x1800aab90) writes through
//     `*(HANDLE*)(this+0x38)` and then adds the byte count to
//     `*(int*)(this+0x2c)`. Against FileInfo at +0x20 those are winHandle
//     (+0x18) and cursor (+0x0C) — the two fields a write would touch, at the
//     two offsets FileSystem.h already asserts, with no fudge factor.
//
//  3. ?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z (0x1800aa8f0)
//     assigns the CreateFileW result into `this+0x20` through FUN_18000c200,
//     which filesystem_openfile_probe.cpp has already identified as
//     `FileInfo::operator=`, and then returns `*(int*)(this+0x20) != -1` —
//     FileInfo::handle's documented "nothing opened" sentinel.
//
//  4. The arithmetic closes exactly: 0x20 + sizeof(FileInfo) 0x30 == 0x50,
//     the measured size, with no tail padding.
//
// THE CONSTRUCTOR LEAVES ONE FIELD UNWRITTEN. ??0FileStream (0x1800aaa10)
// writes +0x18 q, +0x20 d, +0x28 q, +0x30 d, +0x38 q(-1), +0x40 q, +0x48 q.
// That never touches +0x24 — which is FileInfo::_unk04, the field
// FileSystem.h ALREADY records as "never initialised by OpenFile" and which
// filesystem_openfile_win.cpp's oracle already excludes. Two unrelated
// functions leave the same field alone; that agreement is the point.
//
// Recorded rather than zeroed: a `= {}` in a port would be a behavioural
// difference from retail, not tidiness (saveload_meta.cpp takes the same
// position on CompressedStream +0x18/+0x24).
//
// VTABLE 0x180154960, EXPORTED as ??_7FileStream@IO@@6B@ (ordinal 1435) —
// 8 slots. The table ends at +0x38 because 0x180154960+0x40 == 0x1801549a0,
// which Stream.h already pins as ??_7Stream@IO@@6B@:
//   [0] +0x00  0x18000c090  deleting destructor thunk
//   [1] +0x08  0x1800aab50  GetSource
//   [2] +0x10  0x1800aab90  Write
//   [3] +0x18  0x1800aabf0  Read
//   [4] +0x20  0x1800aac10  SetCurrentPos
//   [5] +0x28  0x1800aac50  GetCurrentPos
//   [6] +0x30  0x1800aac60  GetSize
//   [7] +0x38  0x1800aac70  Open           <-- FileStream's own first virtual
//
// NOTE ON SLOT 7. Stream.h says "derived classes add Close at +0x38 and
// PreCommit at +0x40", which is true of the MemoryStream/CompressedStream
// branch and NOT of this one: IO::Stream declares only 7 virtuals, so slot 7
// is whatever the derived class declares first. MemoryStream puts Close there;
// FileStream puts Open there. The two are not the same slot meaning, and a
// port that dispatched +0x38 on a FileStream expecting Close would call Open.
// ============================================================
struct FileStream {
    Stream           base;       // +0x00
    char*            filename;   // +0x18  System::String, refcounted
    RtIO::FileInfo   file;       // +0x20
};
static_assert(offsetof(FileStream, filename) == 0x18,
              "FileStream.filename @ 0x18 (~FileStream 0x1800aaad4 releases it)");
// VERIFIED BY AN INDEPENDENT ROUTE, not merely asserted. Ghidra's decompiler on
// ?Write@FileStream@IO@@UEAA_NHPEBX@Z (0x1800aab90) -- a body that has nothing to
// do with the destructor this claim was originally read off -- gives
//     WriteFile(*(HANDLE *)(this + 0x38), buf, len, &written, 0);
//     *(DWORD *)(this + 0x2c) = *(int *)(this + 0x2c) + written;
// so +0x38 is the HANDLE and +0x2c is the byte counter. Against `FileInfo` at
// +0x20 those are winHandle (+0x18) and cursor (+0x0C) at exactly the offsets
// FileSystem.h already asserts: ONE ALREADY-PINNED STRUCT PREDICTING TWO
// OFFSETS, BOTH LANDING. A third route agrees from a third direction --
// ?SaveText@External@Object@@ (0x1800bb9d0) reaches the same two fields from a
// frame pointer of rsp-0x37, so every offset in its epilogue is odd and none of
// it could have been copied from either of the other two bodies.
static_assert(offsetof(FileStream, file)     == 0x20,
              "FileStream.file @ 0x20 (~FileStream passes this+0x20 to CloseFile; "
              "confirmed independently by ?Write@FileStream's +0x38/+0x2c pair "
              "and again by ?SaveText@External's epilogue)");
static_assert(offsetof(FileStream, file) + offsetof(RtIO::FileInfo, cursor) == 0x2c,
              "?Write@FileStream 0x1800aabb8: `ADD dword [RCX+0x2c],EAX`; and "
              "?GetCurrentPos@FileStream 0x1800aac50 is `MOV EAX,[RCX+0x2c]; RET`");
static_assert(offsetof(FileStream, file) + offsetof(RtIO::FileInfo, size) == 0x28,
              "?GetSize@FileStream 0x1800aac60: `MOV EAX,dword ptr [RCX+0x28]; RET`");
static_assert(offsetof(FileStream, file) + offsetof(RtIO::FileInfo, winHandle) == 0x38,
              "?Write@FileStream 0x1800aaba0: `MOV RCX,[RCX+0x38]` into WriteFile");
static_assert(sizeof(FileStream) == 0x50,
              "sizeof(IO::FileStream) == 0x50 (eh_vector_dtor stride at 0x180023d80)");

// ============================================================
// IO::BinaryFileStream — 0x50 bytes. A FileStream and NOTHING ELSE.
//
// ??0BinaryFileStream@IO@@QEAA@XZ (0x1800a8200) runs the FileStream ctor
// inline, stamps its own vtable, and then installs an Encoding at +0x10 — a
// Stream field, not a new one. It writes no offset >= 0x50, and sizeof is the
// same 0x50 the FileStream thunk reports (the two classes share the deleting
// destructor thunk at 0x180023d80, which is itself the tell).
//
// The class exists to swap the vtable, and the ONLY slot it swaps is the
// destructor. Its table is byte-identical to FileStream's in slots 1..7:
//
//   ??_7BinaryFileStream@IO@@6B@ 0x1801554d0 (ordinal 1390) — 8 slots
//   [0] +0x00  0x180023d80  deleting dtor   <-- the only difference
//   [1..7]     identical to ??_7FileStream@IO@@6B@ above
//
// It ends at +0x38 because 0x1801554d0+0x40 == 0x180155510 ==
// ??_7CompressedCopyFileStream@IO@@6B@ (ordinal 1407) — the same adjacency
// argument compressedstream.cpp:725 uses to bound the table above it.
//
// DECLARED AS A DISTINCT TYPE ANYWAY, not a typedef, because the vtable it
// carries is a different pointer and a caller that stamps the wrong one gets a
// destructor mismatch that nothing else would catch.
// ============================================================
struct BinaryFileStream {
    FileStream base;  // +0x00 — and that is the whole class
};
static_assert(sizeof(BinaryFileStream) == 0x50,
              "sizeof(IO::BinaryFileStream) == 0x50 (eh_vector_dtor stride 0x180023d80)");

// ============================================================
// IO::CompressedFileStream — 0xb8 bytes.
//
//   +0x00..0x67  CompressedStream base
//   +0x68..0xb7  BinaryFileStream _file   (a MEMBER, not a second base)
//
// THE SAME SHAPE AS CompressedMemoryStream, one class over: a CompressedStream
// whose _source is a concrete stream embedded in the same object. Stream.h
// already spells that pattern out at +0x68 for the memory case; this is the
// file case, at the same offset, for the same reason (CompressedStream is 0x68
// bytes, so the member lands right after it).
//
// PINNED BY THE CONSTRUCTOR, ??0CompressedFileStream@IO@@QEAA@XZ (0x1800a93b0),
// which is three lines:
//     CompressedStream::CompressedStream(this);
//     this->vptr = &CompressedFileStream::_vftable_;
//     BinaryFileStream::BinaryFileStream(this + 0x68);
//
// AND INDEPENDENTLY BY ITS ONE REAL CALLER. ?Load@External@Object@@
// (0x1800bc620) does not call that ctor — MSVC inlined it — and the inlined
// copy constructs the BinaryFileStream into a stack slot exactly 0x68 bytes
// above the CompressedStream's. Its destructor epilogue then reads the
// FileInfo handle at (object + 0x68 + 0x20) == +0x88 before calling
// FileSystem::CloseFile on it. Both agree with +0x68, from a body that has no
// idea what the constructor looked like.
//
// SIZE: 0x68 + 0x50 == 0xb8, which is the measured eh_vector stride. No padding.
//
// VTABLE 0x1801555f0, EXPORTED as ??_7CompressedFileStream@IO@@6B@
// (ordinal 1408) — 9 slots, the CompressedStream shape (Close at +0x38,
// PreCommit at +0x40), NOT the FileStream shape:
//   [0] +0x00  0x180024100  deleting dtor
//   [1] +0x08  0x18000c6a0  GetSource      (Stream's)
//   [2] +0x10  0x1800aa6c0  Write          (CompressedStream's)
//   [3] +0x18  0x1800aa780  Read           (CompressedStream's)
//   [4] +0x20  0x1800aa830  SetCurrentPos  (CompressedStream's)
//   [5] +0x28  0x1800aa850  GetCurrentPos  (CompressedStream's)
//   [6] +0x30  0x1800aa860  GetSize        (CompressedStream's)
//   [7] +0x38  0x1800a9490  Close          <-- ?Close@CompressedFileStream, its own
//   [8] +0x40  0x1800aa8c0  PreCommit      (CompressedStream's)
//
// TWO vtables live in one object: this one at +0x00 and ??_7BinaryFileStream
// at +0x68. That is not the two-vtables-on-one-CLASS hazard Wave 11 tracks —
// they are on two different sub-objects at two different offsets, and each
// ctor stamps its own.
// ============================================================
struct CompressedFileStream {
    CompressedStream base;  // +0x00
    BinaryFileStream file;  // +0x68
};
static_assert(offsetof(CompressedFileStream, file) == 0x68,
              "CompressedFileStream.file @ 0x68 (ctor 0x1800a93c8: LEA RCX,[RBX+0x68])");
static_assert(sizeof(CompressedFileStream) == 0xb8,
              "sizeof(IO::CompressedFileStream) == 0xb8 (eh_vector_dtor stride 0x180024100)");

// ============================================================
// IO::TextFileStream — 0x80 bytes. A FileStream that writes/parses text.
//
//   +0x00..0x4f  FileStream
//   +0x50  int32   _objectVersion   SetObject's third argument
//   +0x54  (never written by either constructor)
//   +0x58  String  _objectName      SetObject's SECOND argument  <-- see below
//   +0x60  String  _objectType      SetObject's FIRST argument   <-- see below
//   +0x68  int32   _requestedFormat ctor: 6
//   +0x6c  int32   _activeEncoding  ctor: 6        <-- the BOM selector
//   +0x70  void*   _buffer          owned, plain MemFree in the dtor
//   +0x78  void*   (ctor zeroes it; not freed by the destructor)
//
// PINNED BY BOTH CONSTRUCTORS AND BY ITS CALLER, which is worth having because
// the two constructors disagree by exactly one store:
//
//   ??0TextFileStream@IO@@QEAA@XZ                    (0x1800ba530) writes +0x50 = 0
//   ??0TextFileStream@IO@@QEAA@AEBVString@..@0H@Z    (0x1800ba340) does NOT,
//       and instead tail-calls SetObject(type, name, version) — which is what
//       sets +0x50, +0x58 and +0x60. So +0x50 is the VERSION, not a flag: the
//       default ctor's 0 and SetObject's third argument write the same slot.
//
// +0x58 vs +0x60 — THE TYPE AND THE NAME ARE THE OTHER WAY ROUND FROM THE
// OBVIOUS READING, and this header had them swapped until retail's own
// accessors were run against it:
//
//     ?GetObjectType@TextFileStream@IO@@QEBAAEBVString@System@@XZ   0x1800ba260
//         is `LEA RAX,[RCX + 0x60] ; RET`
//     ?GetObjectName@TextFileStream@IO@@QEBAAEBVString@System@@XZ   0x1800ba270
//         is `LEA RAX,[RCX + 0x58] ; RET`
//     ?GetObjectVersion@TextFileStream@IO@@QEBAHXZ                  0x1800ba250
//         is `MOV EAX,dword ptr [RCX + 0x50] ; RET`
//
// and ?SetObject@ (0x1800ba280) agrees from the write side, independently:
// its FIRST String argument lands at +0x60 and its SECOND at +0x58, with the
// int at +0x50. Two unrelated bodies, same answer.
//
// The swap was not catchable by inspection. Both fields are refcounted
// `String`s written by the same idiom in the same function, both are read by
// one-instruction accessors, and the constructor's argument list is
// `(String const&, String const&, int)` with no name attached to either. A pair
// of same-typed adjacent fields is a coin flip, and the coin came up wrong
// here: the only thing that settles it is a body that NAMES one of them, which
// is what the accessors do. Recorded rather than quietly fixed because
// "guessed a symmetric pair" is a defect class, not a slip.
//
// +0x68 vs +0x6c, which are both 6 and would otherwise be indistinguishable:
// ?SaveText@External@Object@@ (0x1800bb9d0) writes BOTH to 2 and then branches
// on +0x6c ALONE to decide the byte-order mark —
//     3 -> EF BB BF (UTF-8)   4 -> FF FE   5 -> FE FF   0 or 2 -> none
// — so +0x6c is the encoding actually in force and +0x68 is the one requested.
// Nothing this lane touches writes them to different values; the distinction is
// recorded because guessing them the same way round is a coin flip.
//
// THE DESTRUCTOR IS WHAT PROVES THE THREE POINTERS ARE DIFFERENT KINDS.
// SaveText's inlined epilogue (and ??1TextFileStream 0x1800ba490) frees +0x70
// with a BARE MemFree, and releases +0x58 and +0x60 through the refcounted
// String idiom (decrement the int at p[-4], free p-8 at zero). +0x78 is
// touched by neither. A port that used one idiom for all three would leak two
// strings and double-free a buffer.
//
// VTABLE 0x180155980, EXPORTED as ??_7TextFileStream@IO@@6B@ (ordinal 1516) —
// 10 slots. It is bounded not by the next class's table but by an ENCODING
// table: 0x180155980+0x50 == 0x1801559d0, which is the very Encoding vtable
// this class's own constructor installs at +0x10. That is a tighter bound than
// adjacency-to-a-class, because the constructor names the address.
//   [0] +0x00  0x18002ba90  deleting dtor
//   [1] +0x08  0x1800aab50  GetSource       (FileStream's)
//   [2] +0x10  0x1800ba9f0  Write
//   [3] +0x18  0x1800ba900  Read
//   [4] +0x20  0x1800babd0  SetCurrentPos
//   [5] +0x28  0x1800bac30  GetCurrentPos
//   [6] +0x30  0x1800ba8e0  GetSize
//   [7] +0x38  0x1800aac70  Open            (FileStream's — slot 7 is Open here)
//   [8] +0x40  0x1800ba640  Open(Filename, Text::Format)   <-- its own overload
//   [9] +0x48  0x1800ba940  Create(Filename, Text::Format, Filename)
// ============================================================
struct TextFileStream {
    FileStream base;             // +0x00
    int32_t    objectVersion;    // +0x50
    int32_t    _unk54;           // +0x54 — never written by either ctor
    char*      objectName;       // +0x58  System::String, refcounted
    char*      objectType;       // +0x60  System::String, refcounted
    int32_t    requestedFormat;  // +0x68  ctor: 6
    int32_t    activeEncoding;   // +0x6c  ctor: 6 — the BOM selector
    void*      buffer;           // +0x70  owned, bare MemFree
    void*      _unk78;           // +0x78  zeroed by the ctor, never freed
};
static_assert(offsetof(TextFileStream, objectVersion)   == 0x50,
              "??0TextFileStream 0x1800ba55d writes +0x50 = 0; SetObject writes the version there");
static_assert(offsetof(TextFileStream, objectName)      == 0x58,
              "?GetObjectName@TextFileStream 0x1800ba270: `LEA RAX,[RCX+0x58]`");
static_assert(offsetof(TextFileStream, objectType)      == 0x60,
              "?GetObjectType@TextFileStream 0x1800ba260: `LEA RAX,[RCX+0x60]`");
static_assert(offsetof(TextFileStream, requestedFormat) == 0x68, "ctor: `MOV dword [RCX+0x68],6`");
static_assert(offsetof(TextFileStream, activeEncoding)  == 0x6c,
              "ctor: `MOV dword [RCX+0x6c],6`; SaveText branches on THIS one for the BOM");
static_assert(offsetof(TextFileStream, buffer)          == 0x70,
              "~TextFileStream frees +0x70 with a bare MemFree, not the String idiom");
static_assert(sizeof(TextFileStream) == 0x80,
              "sizeof(IO::TextFileStream) == 0x80 (eh_vector_dtor stride 0x18002ba90)");

// ============================================================
// IO::VisitorStream — 0x18 bytes. A Stream and NOTHING ELSE.
//
// This is the smallest class in the family and the most surprising one, so the
// claim is stated plainly: VisitorStream adds NO fields to IO::Stream, and its
// stream methods do nothing.
//
//   ??0VisitorStream@IO@@QEAA@XZ (0x1800bb090): run the Stream ctor, stamp its
//     own vtable, set _direction = 1 (writing), install an Encoding at +0x10
//     whose vtable is 0x1801559f8. No other store.
//   ??1VisitorStream@IO@@UEAA@XZ (0x1800bb040): stamp its vtable, stamp
//     Stream's, delete the Encoding at +0x10. No other read.
//   ?Write@VisitorStream@IO@@UEAA_NHPEBX@Z (0x1800bb030): `return true;`
//     — the entire body. It accepts every byte and stores none.
//
// IT IS A NULL SINK, and that is the mechanism of the whole reference-visit
// pass. `ExternalFactory::Add` constructs one and then calls the External's
// own Serialize (vtable +0x28) against it. The object walks itself exactly as
// it would when saving, every byte it emits is swallowed, and the only thing
// that actually happens is the side effect in the Encoding at +0x10 — the
// Visitor encoding at 0x1801559f8, which records the resource references the
// walk passes through. Serializer.h:154 and serialize_family.cpp:125 both
// describe that pass from the other end; this is the stream half of it.
//
// SIZE 0x18 is measured (eh_vector stride at 0x18002bf80) and cross-checked by
// its one caller: Add's stack slot spans exactly vptr / direction / encoding
// and the inlined destructor reads only +0x10.
//
// VTABLE 0x180155a20, EXPORTED as ??_7VisitorStream@IO@@6B@ (ordinal 1528) —
// 7 slots, the bare IO::Stream shape with no Open, Close or PreCommit. It is
// bounded by a STRING: the qword at 0x180155a58 is ASCII "External", i.e. the
// table runs straight into .rdata string data.
//   [0] +0x00  0x18002bf80  deleting dtor
//   [1] +0x08  0x18000c6a0  GetSource      (Stream's)
//   [2] +0x10  0x1800bb030  Write          — `return true`
//   [3] +0x18  0x1800bb020  Read
//   [4] +0x20  0x1800bb010  SetCurrentPos
//   [5] +0x28  0x1800bb000  GetCurrentPos
//   [6] +0x30  0x1800baff0  GetSize
//
// DO NOT CONFUSE THIS WITH IO::Visitor (??_7Visitor@IO@@6B@, 0x1801558b0),
// which is a 24+-slot SERIALIZER subclass with the whole Serialize overload
// set. VisitorStream is the Stream; Visitor is the Serializer that rides on it.
// The names are one token apart and the two are not related by inheritance.
// ============================================================
struct VisitorStream {
    Stream base;  // +0x00 — and that is the whole class
};
static_assert(sizeof(VisitorStream) == 0x18,
              "sizeof(IO::VisitorStream) == 0x18 (eh_vector_dtor stride 0x18002bf80)");

// ---------------------------------------------------------------------------
// The vtable addresses this family stamps, so a caller reproducing an inlined
// constructor or destructor names them once rather than spelling a literal at
// each store. All five are EXPORTED, so a port should reach them by name
// (RT_VTABLE_ONCE / RtResolveCached) rather than by address.
// ---------------------------------------------------------------------------
constexpr uintptr_t kAddrVtblFileStream           = 0x180154960;  // ??_7FileStream@IO@@6B@
constexpr uintptr_t kAddrVtblBinaryFileStream     = 0x1801554d0;  // ??_7BinaryFileStream@IO@@6B@
constexpr uintptr_t kAddrVtblCompressedFileStream = 0x1801555f0;  // ??_7CompressedFileStream@IO@@6B@
constexpr uintptr_t kAddrVtblTextFileStream       = 0x180155980;  // ??_7TextFileStream@IO@@6B@
constexpr uintptr_t kAddrVtblVisitorStream        = 0x180155a20;  // ??_7VisitorStream@IO@@6B@

// FileStream's slot 7 is Open, NOT Close. Named so a call site cannot repeat
// the mistake the comment on FileStream warns about; Stream.h's
// kStreamVtblCloseSlot is 7 as well and means something else on that branch.
constexpr size_t kFileStreamVtblOpenSlot = 7;

}  // namespace RtIo

// ===========================================================================
// ENDGAME B.3.4 block 4 (STREAMS) — the REAL `IO::FileStream` and
// `IO::BinaryFileStream`.
// ===========================================================================
//
// Design: B34_RULES.md section 1.3 (the slot lists and the slot-7 trap),
// section 4.1 (`NoteSlotDispatch` dies with this commit), section 4.3 (no real
// constructors). Bodies: runtime/src/io/file_stream.cpp and file_stream_win.cpp.
// The rules that govern the whole family are stated once, in Stream.h's
// `namespace IO` banner; only what is specific to this pair is repeated here.
//
// ---------------------------------------------------------------------------
// SLOT 7 IS `Open`, AND IT IS THE ONE PLACE THE FAMILY DISAGREES WITH ITSELF
// ---------------------------------------------------------------------------
// `IO::Stream` declares seven virtuals, so slot 7 is whatever the derived class
// declares FIRST. On the MemoryStream / CompressedStream branch that is
// `Close()`; here it is `Open(const System::Filename&)`. A port that dispatched
// +0x38 on a FileStream expecting `Close` would call `Open` — same offset, same
// table shape, completely different behaviour, and nothing would diagnose it.
// The C++ declaration below is what now makes the two unmistakable.
//
// `IO::TextFileStream` then OVERLOADS `Open` across the base/derived boundary
// (its own `(Filename, Format)` form is slot 8), which is why section 1.3's
// rule names that one `OpenWithFormat` and leaves this one `Open`.
//
// ---------------------------------------------------------------------------
// `_filename` STAYS A `char*`, AND THAT IS THE B33 DOUBLE-FREE RULE
// ---------------------------------------------------------------------------
// Retail's field is a `System::String` — which is exactly this one pointer. It
// is NOT declared as one here, because a real `System::String` member would
// give `IO::FileStream` a non-trivial implicit destructor that releases the
// buffer, and the ported `~FileStream` door (0x1800aaab0, file_stream.cpp)
// already releases it by hand at retail's position. Two releases is the
// `RtSystem::String` double-free lesson (B33 section 7) with a different name
// on it. The field converts when the ctor/dtor doors do, which section 4.3 puts
// at the end of B.3, not in this step. Bodies reach it as a String through
// `runtime/src/system/pod_bridge.h`, which is the one transitional seam.
// ===========================================================================

namespace IO {

// ---------------------------------------------------------------------------
// IO::FileStream — vtable 0x180154960, 8 slots, 0x50 bytes.
// ---------------------------------------------------------------------------
// ABSTRACT: slot 0 (0x18000c090) is left PURE because its body is the
// by-address registration at file_stream.cpp, filling slot 0 of the live minted
// table (rt_data_mint_tables.inc:2423), and nothing holds a `FileStream` by
// value. `IO::BinaryFileStream` is the one that must be concrete.
class FileStream : public Stream {
public:
    // slot 0  +0x00  0x18000c090 — PURE, body registered by address.
    Stream* DeletingDtor(unsigned flags) override = 0;
    // slot 1  +0x08  0x1800aab50 — an ADD-REF of `_filename`, not a copy.
    System::String GetSource() const override;
    bool     Write(int n, const void* src) override;   // slot 2 +0x10  0x1800aab90
    bool     Read(int n, void* dst) override;          // slot 3 +0x18  0x1800aabf0
    bool     SetCurrentPos(int pos) override;          // slot 4 +0x20  0x1800aac10
    uint32_t GetCurrentPos() const override;           // slot 5 +0x28  0x1800aac50
    uint32_t GetSize() const override;                 // slot 6 +0x30  0x1800aac60
    // slot 7 +0x38 — FileStream's own first virtual. NOT `Close`.
    virtual bool Open(const System::Filename& path);   //              0x1800aac70

    // ---- the 0x50 bytes, at retail's offsets -------------------------------
    char*          _filename;   // +0x18  a System::String — see the banner
    RtIO::FileInfo _file;       // +0x20  0x30 bytes, RtIO::FileInfo

protected:
    ~FileStream() = default;    // decision 2's base shape: non-virtual
};

static_assert(sizeof(FileStream) == 0x50,
              "sizeof(IO::FileStream) == 0x50 (eh_vector_dtor stride 0x180023d80)");
static_assert(offsetof(FileStream, _filename) == 0x18,
              "~FileStream 0x1800aaad4 releases +0x18");
static_assert(offsetof(FileStream, _file) == 0x20,
              "~FileStream passes this+0x20 to CloseFile; ?Write@FileStream's "
              "+0x38/+0x2c pair and ?SaveText@External's epilogue agree");
static_assert(offsetof(FileStream, _file) + offsetof(RtIO::FileInfo, cursor) == 0x2c,
              "?GetCurrentPos@FileStream 0x1800aac50: `MOV EAX,[RCX+0x2c]; RET`");
static_assert(offsetof(FileStream, _file) + offsetof(RtIO::FileInfo, size) == 0x28,
              "?GetSize@FileStream 0x1800aac60: `MOV EAX,[RCX+0x28]; RET`");
static_assert(offsetof(FileStream, _file) + offsetof(RtIO::FileInfo, winHandle) == 0x38,
              "?Write@FileStream 0x1800aaba0: `MOV RCX,[RCX+0x38]` into WriteFile");

// ---------------------------------------------------------------------------
// IO::BinaryFileStream — vtable 0x1801554d0, 8 slots, 0x50 bytes.
// ---------------------------------------------------------------------------
// A `FileStream` AND NOTHING ELSE. It adds no field and overrides exactly ONE
// slot: `??_7BinaryFileStream@` and `??_7FileStream@` are byte-identical in
// slots 1..7 (re-verified from .rdata, B34_RULES section 1.3) and differ only
// at slot 0, 0x180023d80 against 0x18000c090.
//
// THAT IS THE RETAIL FACT `file_stream.cpp`'s `NoteSlotDispatch` EXISTED FOR:
// with two byte-identical tables a per-body counter could not say which row had
// dispatched, so an `if (vptr == ours)` attendance counter stood in for the
// answer. The question is answered STRUCTURALLY once the two classes exist —
// there is one table per class, the compiler's — so section 4.1 retires the
// whole instrument (the struct, the global, `OurBfsTable`, `NoteBfsStamp`,
// `NoteSlotDispatch`, all seven call sites and `RtIoBfsPromoteCounters`) in
// this commit rather than leaving it counting a constant.
//
// ⚠⚠ NOT CONSTRUCTIBLE IN B.3.4 — live objects are built by the ported ctor
// doors and wear the MINTED table; see B34_RULES section 4.3. ⚠⚠
//
// It is CONCRETE because `IO::CompressedFileStream` holds one BY VALUE at
// +0x68, and a member of abstract type is ill-formed. Concrete would otherwise
// mean an implicit PUBLIC default constructor and therefore a silent
// `IO::BinaryFileStream bfs;` stamping GCC's vtable, so the default constructor
// is PROTECTED — exactly as `IO::MemoryStream`'s is, and for the same reason.
// A probe subclass reaches it through inheritance; `CompressedFileStream`
// reaches it as a FRIEND.
class BinaryFileStream : public FileStream {
public:
    // slot 0 +0x00  0x180023d80 — the ONLY difference from FileStream's table.
    Stream* DeletingDtor(unsigned flags) override;
    // PUBLIC and non-virtual (B34_BRIEFS section A.3): CompressedFileStream
    // holds one by value and its implicit destructor must be able to run this.
    ~BinaryFileStream() = default;

protected:
    BinaryFileStream() = default;          // see the banner — NOT public
    friend class CompressedFileStream;     // the by-value holder, +0x68
};

static_assert(sizeof(BinaryFileStream) == 0x50,
              "IO::BinaryFileStream adds no field: same 0x50 as IO::FileStream");
static_assert(sizeof(BinaryFileStream) == sizeof(FileStream),
              "the class exists to swap slot 0, not to add state");

// ---- the duplicate-POD cross-asserts (B34_RULES section 1.5.3) -------------
static_assert(sizeof(FileStream) == sizeof(RtIo::FileStream),
              "IO::FileStream and the RtIo::FileStream POD stay interchangeable");
static_assert(sizeof(BinaryFileStream) == sizeof(RtIo::BinaryFileStream),
              "IO::BinaryFileStream and its POD stay interchangeable");
static_assert(offsetof(FileStream, _filename) ==
                  offsetof(RtIo::FileStream, filename), "alias filename");
static_assert(offsetof(FileStream, _file) ==
                  offsetof(RtIo::FileStream, file), "alias file");
static_assert(std::is_trivially_destructible<RtIo::FileStream>::value,
              "the POD must stay trivially destructible while both views are live");
static_assert(std::is_trivially_destructible<RtIo::BinaryFileStream>::value,
              "the POD must stay trivially destructible while both views are live");

}  // namespace IO

// ===========================================================================
// ENDGAME B.3.4 block 4 (STREAMS) — `IO::CompressedFileStream`.
// ===========================================================================
// It lives in THIS header rather than beside its base in Stream.h for one
// reason: it holds an `IO::BinaryFileStream` by value, and that class is
// declared here. Stream.h cannot include FileStream.h (FileStream.h already
// includes Stream.h), so the class follows its member.
//
// Section D binds it: this is the class `Object::External::Load` stack-builds,
// and it sits directly on the intermittent compressed-load defect. The one
// body that moved (slot 7, `Close`) moved byte-faithfully.

namespace IO {

// ---------------------------------------------------------------------------
// IO::CompressedFileStream — vtable 0x1801555f0, 9 slots, 0xb8 bytes.
// ---------------------------------------------------------------------------
// THE SAME SHAPE ONE CLASS OVER: a CompressedStream whose `_source` is a
// concrete stream embedded in the same object, here a `BinaryFileStream` at
// +0x68 (0x68 + 0x50 == 0xb8, the measured stride, no padding).
//
// IT OVERRIDES EXACTLY ONE SLOT. Slot 7 is its own `Close` (0x1800a9490);
// slot 8 `PreCommit` is CompressedStream's (0x1800aa8c0), inherited. That
// asymmetry with CompressedMemoryStream, which overrides both, is retail's.
//
// ⚠ ITS CONSTRUCTION SITES ARE NOT IN THIS BLOCK'S FILE SET. Retail inlines
// this constructor into `External::Save`, `IsCurrent` and `Load`, and our port
// reproduces that as a stack local plus a bare vptr stamp in
// `runtime/src/object/external_load.cpp` — a file block 4 does not own, and one
// that is itself on the section D loader path. That is the fact that makes
// section 4.3's "no real constructors" non-negotiable rather than cautious.
class CompressedFileStream : public CompressedStream {
public:
    Stream* DeletingDtor(unsigned flags) override = 0;   // slot 0  0x180024100
    void Close() override;                               // slot 7  0x1800a9490
    // slot 8 PreCommit is NOT overridden: 0x1801555f0[8] is 0x1800aa8c0.

    BinaryFileStream _file;    // +0x68 — a MEMBER, not a second base

protected:
    ~CompressedFileStream() = default;
};

static_assert(offsetof(CompressedFileStream, _file) == 0x68,
              "ctor 0x1800a93c8: `LEA RCX,[RBX+0x68]` before the BinaryFileStream ctor");
static_assert(sizeof(CompressedFileStream) == 0xb8,
              "0xb8 is the eh_vector_dtor element stride at 0x180024100");

static_assert(sizeof(CompressedFileStream) == sizeof(RtIo::CompressedFileStream),
              "IO::CompressedFileStream and its POD stay interchangeable");
static_assert(offsetof(CompressedFileStream, _file) ==
                  offsetof(RtIo::CompressedFileStream, file), "alias member");
static_assert(std::is_trivially_destructible<RtIo::CompressedFileStream>::value,
              "the POD must stay trivially destructible while both views are live");

}  // namespace IO
