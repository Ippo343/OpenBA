// ============================================================================
// tests/unit/io_stream_abi_stubs.cpp — the out-of-line virtuals the REAL
// `IO` stream classes own, defined for the host link only.
// ENDGAME B.3.4 block 4 (STREAMS).
// ============================================================================
//
// WHY THIS FILE EXISTS, AND WHY IT IS NOT A MIRROR
// ------------------------------------------------
// `tests/unit/io_vtable_abi_test.cpp` PART G pins the real stream classes'
// vtable order by constructing a concrete probe subclass of each and
// raw-calling every slot. Constructing one runs the BASE's constructor, which
// stores the base's vptr — so the link needs `vtable for IO::Stream`,
// `vtable for IO::MemoryStream` and so on.
//
// GCC emits a class's vtable in the translation unit that defines its KEY
// FUNCTION (the first non-inline, non-pure virtual), and for these classes that
// is `runtime/src/io/memory_stream.cpp`, `file_stream.cpp`,
// `compressedstream.cpp` and `text_file_stream_win.cpp` — files that stand on
// the port registry, the allocator and <windows.h> and cannot link into a host
// test (tests/unit/README.md, B31_RULES rule 6).
//
// THE CLASS IS STILL THE REAL ONE. What is stubbed here is only the BODY of
// each member: the declarations, their order and their signatures come from
// runtime/types/io/Stream.h and FileStream.h, which is exactly what PART G is
// testing. Every probe overrides every slot, so not one of these bodies is ever
// entered — each one aborts if it is, so "the probe forgot to override a slot"
// fails loudly instead of silently passing through a stub.
//
// This is the opposite of a mirror class. A mirror would re-declare the slots
// and then test its own copy of the order; this file re-declares NOTHING.
//
// IF ONE OF THOSE `runtime/src/io/*.cpp` FILES IS EVER ADDED TO
// tests/unit/sources.mk, the linker will report a duplicate definition here.
// That is the correct signal: delete the stub, not the real body.
// ============================================================================

#include "runtime/types/io/Stream.h"
#include "runtime/types/io/FileStream.h"
#include "runtime/types/io/PackageFile.h"

#include <cstdio>
#include <cstdlib>

namespace {

[[noreturn]] void StubReached(const char* what) {
    std::fprintf(stderr,
                 "\n[unit] io_stream_abi_stubs: %s was actually CALLED.\n"
                 "[unit] A PART G probe is missing an override for that slot,\n"
                 "[unit] so a raw slot call fell through to the host stub.\n\n",
                 what);
    std::fflush(stderr);
    std::abort();
}

}  // namespace

namespace IO {

// ---- IO::MemoryStream (runtime/src/io/memory_stream.cpp) -------------------
// Slot 0 is the class's own inline DeletingDtor (ENDGAME B.3.9 IO).
bool     MemoryStream::Write(int, const void*) { StubReached("MemoryStream::Write"); }
bool     MemoryStream::Read(int, void*)        { StubReached("MemoryStream::Read"); }
bool     MemoryStream::SetCurrentPos(int)      { StubReached("MemoryStream::SetCurrentPos"); }
uint32_t MemoryStream::GetCurrentPos() const   { StubReached("MemoryStream::GetCurrentPos"); }
uint32_t MemoryStream::GetSize() const         { StubReached("MemoryStream::GetSize"); }
void     MemoryStream::Close()                 { StubReached("MemoryStream::Close"); }

// ---- IO::FileStream (runtime/src/io/file_stream.cpp + file_stream_win.cpp) --
// Slot 0 is PURE on this class, so there is nothing to stub for it.
System::String FileStream::GetSource() const     { StubReached("FileStream::GetSource"); }
bool     FileStream::Write(int, const void*)     { StubReached("FileStream::Write"); }
bool     FileStream::Read(int, void*)            { StubReached("FileStream::Read"); }
bool     FileStream::SetCurrentPos(int)          { StubReached("FileStream::SetCurrentPos"); }
uint32_t FileStream::GetCurrentPos() const       { StubReached("FileStream::GetCurrentPos"); }
uint32_t FileStream::GetSize() const             { StubReached("FileStream::GetSize"); }
bool     FileStream::Open(const System::Filename&) { StubReached("FileStream::Open"); }

// ---- IO::BinaryFileStream --------------------------------------------------
// Its ONE override, slot 0, whose real body is the registered thunk at
// 0x180023d80 in file_stream.cpp.
// ENDGAME B.3.9 IO: BinaryFileStream::DeletingDtor is inline now (retail's
// vector deleting dtor). The three out-of-line lifecycle bodies below are
// file_stream.cpp's, which stands on the FileSystem singleton and the port
// registry; the host versions touch no file. ~FileStream's host body releases
// nothing because no probe opens a file or names one. The TextFileStream
// ctor makes retail's own-field stores but installs NO encoding and skips
// SetObject (the probes read neither).
FileStream::~FileStream() {}
TextFileStream::TextFileStream(const System::String&, const System::String&, int)
    : _objectName(nullptr), _objectType(nullptr), _requestedFormat(6),
      _activeEncoding(6), _buffer(nullptr), _bufSize(0), _bufCursor(0) {}
TextFileStream::~TextFileStream() {}

// ---- IO::CompressedStream (compressed_stream_{write,read}.cpp) -------------
// Slot 0 is PURE on CompressedStream / CompressedMemoryStream and inline on
// CompressedFileStream (B.3.9 IO), so none of them needs one.
bool     CompressedStream::Write(int, const void*) { StubReached("CompressedStream::Write"); }
bool     CompressedStream::Read(int, void*)        { StubReached("CompressedStream::Read"); }
bool     CompressedStream::SetCurrentPos(int)      { StubReached("CompressedStream::SetCurrentPos"); }
uint32_t CompressedStream::GetCurrentPos() const   { StubReached("CompressedStream::GetCurrentPos"); }
uint32_t CompressedStream::GetSize() const         { StubReached("CompressedStream::GetSize"); }
void     CompressedStream::Close()                 { StubReached("CompressedStream::Close"); }
void     CompressedStream::PreCommit()             { StubReached("CompressedStream::PreCommit"); }

// ---- the two subclasses' overrides ----------------------------------------
void CompressedMemoryStream::Close()     { StubReached("CompressedMemoryStream::Close"); }
void CompressedMemoryStream::PreCommit() { StubReached("CompressedMemoryStream::PreCommit"); }
void CompressedFileStream::Close()       { StubReached("CompressedFileStream::Close"); }

// ENDGAME B.3.9 IO: the two out-of-line destructors (compressed_stream_release.cpp,
// file_stream.cpp) stand on Release / Commit / the FileSystem singleton and the
// port registry. No probe writes a header, opens a file or grows an array, so
// the host bodies have nothing to release; ~Stream still frees the encoding.
CompressedStream::~CompressedStream() {}
CompressedFileStream::~CompressedFileStream() {}

// ---- IO::TextFileStream (runtime/src/io/text_file_stream_win.cpp) ----------
// Slot 0 is PURE on this class. Slots 1 and 7 are FileStream's, above. What is
// left is the seven this class overrides or adds — note that slot 8 is
// `OpenWithFormat`, NOT a second `Open`: retail overloads `Open` across the
// base/derived boundary and B34_RULES section 1.3 gives the derived one its own
// name so the two slots cannot be confused.
bool     TextFileStream::Write(int, const void*) { StubReached("TextFileStream::Write"); }
bool     TextFileStream::Read(int, void*)        { StubReached("TextFileStream::Read"); }
bool     TextFileStream::SetCurrentPos(int)      { StubReached("TextFileStream::SetCurrentPos"); }
uint32_t TextFileStream::GetCurrentPos() const   { StubReached("TextFileStream::GetCurrentPos"); }
uint32_t TextFileStream::GetSize() const         { StubReached("TextFileStream::GetSize"); }
bool     TextFileStream::OpenWithFormat(const System::Filename&, int32_t) {
    StubReached("TextFileStream::OpenWithFormat");
}
bool     TextFileStream::Create(const System::Filename&, int32_t, int32_t) {
    StubReached("TextFileStream::Create");
}

// ---- IO::VisitorStream (runtime/src/io/iobase_vtables.cpp) -----------------
// The null sink. Slot 0 is PURE and slot 1 is IO::Stream's own GetSource, so
// these five are the whole class.
bool     VisitorStream::Write(int, const void*) { StubReached("VisitorStream::Write"); }
bool     VisitorStream::Read(int, void*)        { StubReached("VisitorStream::Read"); }
bool     VisitorStream::SetCurrentPos(int)      { StubReached("VisitorStream::SetCurrentPos"); }
uint32_t VisitorStream::GetCurrentPos() const   { StubReached("VisitorStream::GetCurrentPos"); }
uint32_t VisitorStream::GetSize() const         { StubReached("VisitorStream::GetSize"); }

// ---- IO::PackageFile (runtime/src/io/package_file*.cpp) --------------------
// ENDGAME B.3.9 IO: the class is concrete, so the b39_io_packagefile test
// builds the REAL class and the link needs its vtable (key function
// TypeName, in package_file.cpp, which stands on the port registry). The
// constructor and slot 0 are inline and real; the destructor's host body
// releases nothing because a freshly built package holds nothing.
const char*   PackageFile::TypeName() const             { StubReached("PackageFile::TypeName"); }
uint32_t      PackageFile::Type() const                 { StubReached("PackageFile::Type"); }
int32_t       PackageFile::GetFileVersion() const       { StubReached("PackageFile::GetFileVersion"); }
Object::External::CompileResult PackageFile::Compile(const System::Filename&) {
    StubReached("PackageFile::Compile");
}
bool          PackageFile::Serialize(void*)             { StubReached("PackageFile::Serialize"); }
Object::External* PackageFile::Reconstruct()            { StubReached("PackageFile::Reconstruct"); }
void          PackageFile::Destruct()                   { StubReached("PackageFile::Destruct"); }
Object::External* PackageFile::Construct()              { StubReached("PackageFile::Construct"); }
PackageFile::~PackageFile() {}

}  // namespace IO
