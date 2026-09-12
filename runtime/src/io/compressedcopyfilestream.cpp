// ============================================================================
// runtime/src/io/compressedcopyfilestream.cpp — VTABLE BATCH 9
//   IO::CompressedCopyFileStream, 4 of 9 vtable slots (0, 2, 3, 7 -- slot 0's
//   own body lives in deleting_dtor.cpp; slots 1, 4, 5, 6, 8 are inherited
//   CompressedStream bodies already ported elsewhere, standalone/
//   rt_data_mint_tables.inc's kVSlots28).
// ============================================================================
//
// ??_7CompressedCopyFileStream@IO@@6B@ (retail 0x180155510), 9 slots:
//   0  deleting dtor    0x1800245f0  deleting_dtor.cpp row (sizeof 0xc8)
//   1  GetSource        0x18000c6a0  already ported (IO::Stream)
//   2  Write            0x1800a90e0  here
//   3  Read             0x1800a9080  here
//   4  SetCurrentPos    0x1800aa830  already ported (CompressedStream)
//   5  GetCurrentPos    0x1800aa850  already ported (CompressedStream)
//   6  GetSize          0x1800aa860  already ported (CompressedStream)
//   7  Close            0x1800a9140  here
//   8  PreCommit        0x1800aa8c0  already ported (CompressedStream)
// plus ??1CompressedCopyFileStream@IO@@UEAA@XZ (0x1800a92a0), the deleting-dtor
// row's dtorVa: stamp own vtable, tail-jump to ~CompressedFileStream
// (0x1800a9330, unexported, reached by name through the port-aware door).
//
// LAYOUT. `CompressedFileStream` is 0xb8 bytes (its own deleting-dtor row);
// this class adds 0x10 more, to 0xc8 (the deleting destructor's own
// `MOV EDX,0xc8`):
//   +0xb8  bool  _hasCompanion   read by all three bodies below
//   +0xc0  void* _companion      a second stream this class mirrors every
//                                Write/Read/Close onto. Not further reversed
//                                here: nothing in this batch's three bodies
//                                needs its layout, only its vtable's slots
//                                0x10 (Write/Read, ONE slot serving both --
//                                same shape as CompressedStream's own
//                                Write/Read sharing a scalar-int slot
//                                elsewhere in this tree) and 0x38 (Close).
//
// EVERY NON-SLOT HELPER THESE THREE BODIES REACH IS ALREADY PORTED:
//   ?Write@CompressedStream@IO@@   0x1800aa6c0  runtime/src/io/compressed_stream_write.cpp
//   ?Read@CompressedStream@IO@@    0x1800aa780  runtime/src/io/compressed_stream_read.cpp
//   ?Commit@CompressedStream@IO@@  0x1800a9ee0  runtime/src/io/compressed_stream_write.cpp
//   ?CloseFile@FileSystem@IO@@     0x1800accc0  runtime/src/io/filesystem_createfile_win.cpp
// so this file, like external_list.cpp in this same batch, is composition
// only.
//
// ---------------------------------------------------------------------------
// ?Write@CompressedCopyFileStream@IO@@UEAA_NHPEBX@Z   0x1800a90e0
// ---------------------------------------------------------------------------
// Read from the disassembly, not the decompile: Ghidra's C drops the two
// forwarded arguments from the companion call entirely (both calls in this
// file print as bare `()` -- reference_ghidra_dropped_args.md, textbook case).
// RDX/R8 are never clobbered between the caller's load and either CALL, so
// both the companion call and the tail call receive the SAME (count, buffer)
// pair the caller passed in:
//
//     if (this->_hasCompanion)
//         companion->vt[0x10](companion, count, buffer);   // side effect only,
//                                                            // result discarded
//     return CompressedStream::Write(this, count, buffer);   // retail TAILS here
//
// ---------------------------------------------------------------------------
// ?Read@CompressedCopyFileStream@IO@@UEAA_NHPEAX@Z    0x1800a9080
// ---------------------------------------------------------------------------
//     bool ok = CompressedStream::Read(this, count, buffer);   // ordinary CALL
//     if (ok && this->_hasCompanion)
//         companion->vt[0x10](companion, count, buffer);
//     return true;                                              // retail: MOV AL,1
//                                                                 // unconditionally,
//                                                                 // not `ok`
//
// ---------------------------------------------------------------------------
// ?Close@CompressedCopyFileStream@IO@@UEAAXXZ   0x1800a9140
// ---------------------------------------------------------------------------
//     companion->vt[0x38](companion);              // Close, no other args
//     if (this->+0x48 > 0 && this->_direction == 1)   // CompressedStream fields
//         CompressedStream::Commit(this);
//     if (this->+0x88 != -1)                           // a FileInfo, embedded
//         FileSystem::CloseFile(FileSystem::_singleton, (FileInfo*)(this+0x88));
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"

namespace RtIo {

namespace {

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

constexpr int kCompanionVtblWrite = 0x10 / 8;   // shared Write/Read slot
constexpr int kCompanionVtblClose = 0x38 / 8;

using WriteFn = bool (*)(void* self, int32_t count, const void* buf);
using ReadFn  = bool (*)(void* self, int32_t count, void* buf);
using CommitFn = void (*)(void* self);
using CloseFileFn = void (*)(void* fileSystemSingleton, void* fileInfo);
using CompanionWriteFn = void (*)(void* companion, int32_t count, const void* buf);
using CompanionCloseFn = void (*)(void* companion);

}  // namespace

// ---- ?Write@CompressedCopyFileStream@IO@@UEAA_NHPEBX@Z   0x1800a90e0 -----
extern "C" bool RtIo_CompressedCopyFileStream_Write(void* selfv, int32_t count,
                                                    const void* buf) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    if (*(p + 0xb8) != 0) {                                       // 1800a90ef
        void* companion = *reinterpret_cast<void**>(p + 0xc0);    // 1800a9100
        reinterpret_cast<CompanionWriteFn>(VTableOf(companion)[kCompanionVtblWrite])
            (companion, count, buf);                              // 1800a910a
    }
    return RT_EXPORT_ONCE(WriteFn, "?Write@CompressedStream@IO@@UEAA_NHPEBX@Z")
        (selfv, count, buf);                                      // 1800a9124 JMP
}

// ---- ?Read@CompressedCopyFileStream@IO@@UEAA_NHPEAX@Z    0x1800a9080 -----
extern "C" bool RtIo_CompressedCopyFileStream_Read(void* selfv, int32_t count,
                                                   void* buf) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    const bool ok = RT_EXPORT_ONCE(ReadFn, "?Read@CompressedStream@IO@@UEAA_NHPEAX@Z")
        (selfv, count, buf);                                      // 1800a9097
    if (ok && *(p + 0xb8) != 0) {                                  // 1800a909e/1800a90a7
        void* companion = *reinterpret_cast<void**>(p + 0xc0);     // 1800a90a9
        reinterpret_cast<CompanionWriteFn>(VTableOf(companion)[kCompanionVtblWrite])
            (companion, count, buf);                               // 1800a90b8
    }
    return true;                                                   // 1800a90c5, unconditional
}

// ---- ?Close@CompressedCopyFileStream@IO@@UEAAXXZ   0x1800a9140 -----------
extern "C" void RtIo_CompressedCopyFileStream_Close(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    void* companion = *reinterpret_cast<void**>(p + 0xc0);         // 1800a9149
    reinterpret_cast<CompanionCloseFn>(VTableOf(companion)[kCompanionVtblClose])
        (companion);                                                // 1800a9153

    if (*reinterpret_cast<int32_t*>(p + 0x48) > 0 &&                // 1800a9156
        *reinterpret_cast<int32_t*>(p + 0x08) == 1) {                // 1800a915c
        RT_EXPORT_ONCE(CommitFn, "?Commit@CompressedStream@IO@@QEAAXXZ")(selfv);  // 1800a9165
    }

    if (*reinterpret_cast<int32_t*>(p + 0x88) != -1) {               // 1800a9171
        void** fsSlot = RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
        void*  fs     = fsSlot ? *fsSlot : nullptr;
        RT_EXPORT_ONCE(CloseFileFn, "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z")
            (fs, p + 0x88);                                          // 1800a9182 JMP
    }
}

}  // namespace RtIo

// ============================================================================
// Registration
// ============================================================================
int RtIoCompressedCopyFileStreamRegister() {
    using namespace RtIo;
    int n = 0;

    RegisterRuntimeExport("?Write@CompressedCopyFileStream@IO@@UEAA_NHPEBX@Z",
                          (void*)&RtIo_CompressedCopyFileStream_Write); ++n;
    RegisterRuntimeAddr(0x1800a90e0ull, (void*)&RtIo_CompressedCopyFileStream_Write);

    RegisterRuntimeExport("?Read@CompressedCopyFileStream@IO@@UEAA_NHPEAX@Z",
                          (void*)&RtIo_CompressedCopyFileStream_Read); ++n;
    RegisterRuntimeAddr(0x1800a9080ull, (void*)&RtIo_CompressedCopyFileStream_Read);

    RegisterRuntimeExport("?Close@CompressedCopyFileStream@IO@@UEAAXXZ",
                          (void*)&RtIo_CompressedCopyFileStream_Close); ++n;
    RegisterRuntimeAddr(0x1800a9140ull, (void*)&RtIo_CompressedCopyFileStream_Close);

    return n;
}
