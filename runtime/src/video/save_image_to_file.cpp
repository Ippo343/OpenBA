// ============================================================================
// runtime/src/video/save_image_to_file.cpp — Wave 18 lane VRES, row 7
//   ?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z
//   retail 0x1800f2840, ordinal 3609.  static bool(const Filename&, int, int, uint*)
// ============================================================================
//
// Writes a 24-bit bottom-up BMP. The one caller is
// `code/Game/Game/w2_takescreenshot.cpp:270`, the screenshot path.
//
// ---------------------------------------------------------------------------
// PORTING THIS DOES NOT CLOSE THE SCREENSHOT'S CUTOVER HOLE. Say it here so a
// reader cannot infer otherwise from the row being ported.
// ---------------------------------------------------------------------------
// The body below reaches four IO functions that nothing in this tree has
// ported, through `RT_EXPORT_ONCE` — and `ResolveExport` falls through to the
// raw DLL implementation for an unported symbol
// (`reference_converting_a_door_is_not_satisfying_it.md`). With the DLL absent
// those four resolve to null and the screenshot path dies exactly as it did
// before this file existed. Porting moves the hole from "no implementation" to
// "an implementation with four unresolved dependencies"; it does not close it.
//
// The four, named so the IO wave inherits them rather than rediscovering them:
//
//     ?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z     0x1800ae430  ord 2130
//     ??0BinaryFileStream@IO@@QEAA@XZ                              0x1800a8200  ord   55
//     ?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z         0x1800aa8f0  ord 1876
//     ?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z            0x1800accc0  ord 1726
//
// plus `??1BinaryFileStream@IO@@UEAA@XZ` (0x180023c80, ord 532), which is a
// five-instruction thunk onto `??1FileStream@IO@@UEAA@XZ` (0x1800aaab0). All
// five are IO rows, and IO is Wave 19's declared subject.
//
// `IEAA` on `?Create@FileStream@` means PROTECTED and `AEAA` on
// `?AdjustDelimiters@Filename@` means PRIVATE — neither means unexported. This
// DLL exports both (`reference_aeaa_means_private_not_unexported.md`), so every
// door below is a NAME door and not one of them is an address door.
//
// ---------------------------------------------------------------------------
// THE WAVE 6 HOLDBACK NOTE THIS REPLACES WAS SELF-CONTRADICTORY
// ---------------------------------------------------------------------------
// `runtime/src/video/buffers.cpp:1415` held this row back with three reasons.
// Re-checked before inheriting any of them
// (`reference_holdback_blockers_are_stale_check_them_first.md`), and the note
// **disagreed with itself inside one comment block**: its opening line says
// *"One caller (code/Game/Game/w2_takescreenshot.cpp, the screenshot path)"*
// and its closing paragraph says *"w2_takescreenshot.cpp calls a different,
// already-ported capture path — so leaving this on the DLL costs nothing."*
// Both halves sat in the same comment for four waves. The call site settles it:
// line 165 resolves this exact mangled name and line 270 calls it. "Grep the
// call site" is the cheap lesson; "the note already refuted itself" is cheaper.
//
// Its other two reasons:
//   * *"file I/O side effect — not diffable."* Not a real blocker: two
//     subjects, two paths, byte-compare the two files. That is the oracle in
//     `video_resources_probe.cpp` case 9.
//   * *"Wave 4 (IO) and System::String territory."* Half stale. The
//     `System::Filename` / `System::String` / allocator half is ported today,
//     and — the part that collapsed most of the work —
//     `runtime/types/io/FileStream.h` has pinned `IO::FileStream` and
//     `IO::BinaryFileStream` completely since Wave 11 lane IOSTREAM: sizeof
//     0x50, `filename` at +0x18, `FileInfo` at +0x20, the Win32 handle at
//     +0x38, the byte cursor at +0x2c. Nothing here re-derives a layout.
//
// ---------------------------------------------------------------------------
// TWO ABI DETAILS THE DISASSEMBLY SETTLES AND A SIGNATURE WOULD GET WRONG
// ---------------------------------------------------------------------------
// 1. FOUR OF THE CALLEES RETURN A CLASS BY VALUE, so each has a HIDDEN RETURN
//    SLOT that does not appear in the C++ signature. Omitting it from the
//    typedef is a wild write that still returns the right answer
//    (`reference_hidden_retslot_wild_write.md`), so the slot is in every
//    typedef below. The disassembly also settles WHERE it goes, which differs
//    between the member and the free functions:
//      * members  (`GetPathFilename`, `GetPathDirectory`): RCX = `this`,
//        RDX = the return slot. 0x1800f288b/0x1800f2887 pass them that way.
//      * free operators (`System::operator+`): RCX = the return slot,
//        RDX and R8 = the operands.
// 2. `?GetPathFilename@` takes a `bool` in R8B, set to 1 here
//    (0x1800f2877 `MOV R8B,0x1`). It does NOT mean "with the extension" — see
//    the next section, which is what the disassembly actually says. The
//    argument name in the typedef below is `requireExtension` for that reason.
//
// ---------------------------------------------------------------------------
// IT SUBSTITUTES THE EXTENSION, IT DOES NOT APPEND ONE
// ---------------------------------------------------------------------------
// The `path` argument is expected to CARRY an extension, and the function
// replaces it. `?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z` with the bool
// SET returns the base name with its extension REMOVED — it scans back for a
// `.` and takes the substring from the last separator up to `dot - 1`
// (0x1800c67bd..0x1800c6810) — and returns EMPTY when there is no `.` at all
// (0x1800c67a5: `TEST R10B,R10B / JZ` then `*ret = nullptr`). So the target is
// `directory + basename + ".bmp"`, and:
//
//   "…\Screenshot0.bmp"  ->  "…\Screenshot0.bmp"   (the real caller; a no-op)
//   "…\shot.png"         ->  "…\shot.bmp"          (extension REPLACED)
//   "…\shot"             ->  "…\.bmp"              (a file literally named .bmp)
//
// The third row is not hypothetical: lane VRES's oracle hit it on its first run
// by handing the function extensionless paths. BOTH sides produced it, which is
// how the evidence pointed at the probe rather than at this body. Recorded here
// because a caller that builds a path without an extension gets a silently
// wrong filename and a `true` return.
//
// PLATFORM. The three `WriteFile` calls are the only Win32 in the body and live
// in `video_resources_win.cpp` behind `RtVideoWriteFileRaw`, so this file stays
// free of <windows.h> per PLAN §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileStream.h"
#include "../../types/system/Filename.h"
#include "../collection/array_internal.h"
#include "../collection/array_destroy_internal.h"

// video_resources_win.cpp. Returns the DWORD `WriteFile` reported, which is what
// retail adds to the cursor; retail pre-zeroes that out-parameter and ignores
// WriteFile's BOOL entirely, so a failed write contributes zero. Reproduced.
extern "C" uint32_t RtVideoWriteFileRaw(void* handle, const void* buf, uint32_t bytes);

namespace {

// ---- the doors, all by NAME ------------------------------------------------
using GetPathFilenameFn  = void* (*)(const void* self, void* ret, bool requireExtension);
using GetPathDirectoryFn = void* (*)(const void* self, void* ret);
using ConcatStrFn        = void* (*)(void* ret, const void* a, const void* b);
using ConcatCStrFn       = void* (*)(void* ret, const void* a, const char* b);
using AdjustDelimFn      = void  (*)(void* self);
using CreatePathFn       = bool  (*)(void* fileSystem, const void* filename);
using StreamCtorFn       = void* (*)(void* self);
using StreamCreateFn     = bool  (*)(void* self, const void* filename);
using StreamDtorFn       = void  (*)(void* self);
using CloseFileFn        = void  (*)(void* fileSystem, void* fileInfo);

void** FileSystemSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
}

// One System::String reference release, the refcounted idiom.
//
// Reused from lane D2 rather than rewritten: `kD_Str_08` is exactly
// {stride 8, one offset at 0}, so `ReleaseStringsCore(slot, 1, &kD_Str_08)` is
// this body's release with lane D2's verification attached.
//
// RETAIL SPELLS IT TWO WAYS IN THIS ONE FUNCTION and they are equivalent. The
// four early releases (0x1800f28d8 onward) call the exported
// `?MemFree@MemoryAllocator@System@@` with no null re-test, because that
// function's own `if (p)` covers it; the releases inside the inlined destructor
// (0x1800f2ba2 onward) inline Enter/Free/Leave behind a `JZ` on the `ADD`'s
// result. Body B takes the second spelling. Same reads, same writes, same
// frees.
void ReleaseString(char** slot) {
    rtcoll::ReleaseStringsCore(reinterpret_cast<uint8_t*>(slot), 1, &rtcoll::kD_Str_08);
}

// ---------------------------------------------------------------------------
// 0x180053da0 — Collection::Array<uint8>::SetSize(newSize, const uint8& fill)
//
// Transcribed in full rather than specialised to the one call this body makes,
// so the shrink and regrow arms are the same code even though this call site
// only ever drives the fresh-array arm.
//
// It is NOT REGISTERED at its address, for the reason array_destroy.cpp gives
// for the eight helpers it also declines to register: retail's only call to it
// here is an in-DLL `rel32` that our registry cannot intercept, nothing in this
// tree resolves 0x180053da0, and a registration with no caller dilutes the
// census's dead-port signal (`reference_uncalled_port_is_not_verified.md`). It
// is also NOT added to lane Q's spec table in array.cpp: that file is another
// lane's, and this is a byte-element instantiation with a FILL, which none of
// its existing rows carries.
//
// The growth rule is retail's, including the branch that looks like a typo and
// is not: appending EXACTLY ONE element to a NON-EMPTY array trebles the
// capacity, while any other growth takes the new size exactly.
void ArraySetSizeByte(rtcoll::ArrayHdr* a, int32_t newSize, uint8_t fill) {
    const int64_t oldSize = a->_size;                    // MOVSXD RBP,[RCX+8]
    if (newSize == oldSize) return;                      // CMP EDI,EBP / JZ

    uint8_t* const old = a->_data;                       // MOV RSI,[RCX]
    a->_size = newSize;
    if (newSize > a->_capacity) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)      // CMP EDI,EAX / TEST EBP,EBP
            a->_capacity = newSize * 3;                  // LEA EAX,[RDI+RDI*2]
        a->_data = static_cast<uint8_t*>(
            rtcoll::MemAlloc(static_cast<uint64_t>(a->_capacity), 1));
    }

    // Retail starts the fill at oldSize when the buffer did NOT move and at 0
    // when it did — `XOR EAX,EAX / CMP RSI,[RBX] / CMOVZ EAX,EBP`. So a
    // reallocated array re-copies its old elements out of the OLD buffer and
    // then fills the tail, in one loop.
    int64_t i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        uint8_t* dst = a->_data ? a->_data + i : nullptr;
        if (!dst) continue;                              // TEST RDX,RDX / JZ
        *dst = (i < oldSize) ? old[i] : fill;
    }

    if (old != a->_data && old != nullptr)
        rtcoll::MemFree(old);
}

}  // namespace

// ============================================================================
// The body
// ============================================================================
bool Video_ImageBuffer_SaveImageToFile(const void* path, int width, int height,
                                       uint32_t* pixels) {
    // ---- 1. build "<dir><name>.bmp" and adjust its delimiters --------------
    //
    // Four temporaries, constructed in this order and released in exactly the
    // reverse one at the end of the block. Each is initialised to null here;
    // retail's are raw stack slots, but every one of the four callees is a
    // constructor writing into its return slot and none reads it first, so the
    // initialisation is unobservable — and leaving them indeterminate would be
    // UB on our side for no gain.
    char* namePart = nullptr;   // retail [RSP+0x30]
    char* dirPart  = nullptr;   // retail [RBP-0x71]
    char* joined   = nullptr;   // retail [RBP-0x61]
    char* withExt  = nullptr;   // retail [RBP-0x69]
    char* target   = nullptr;   // retail [RBP-0x79] — the System::Filename

    RT_EXPORT_ONCE(GetPathFilenameFn, "?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z")(
        path, &namePart, true);
    RT_EXPORT_ONCE(GetPathDirectoryFn, "?GetPathDirectory@Filename@System@@QEBA?AV12@XZ")(
        path, &dirPart);
    RT_EXPORT_ONCE(ConcatStrFn, "??HSystem@@YA?AVString@0@AEBV10@0@Z")(
        &joined, &dirPart, &namePart);
    RT_EXPORT_ONCE(ConcatCStrFn, "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z")(
        &withExt, &joined, ".bmp");

    // 0x1800f28bf: the Filename ADOPTS the concatenation's buffer and takes a
    // reference; it does not copy. `target` stays null when `withExt` is null.
    if (withExt != nullptr) {
        target = withExt;
        ++*reinterpret_cast<int32_t*>(target - 4);
    }
    RT_EXPORT_ONCE(AdjustDelimFn, "?AdjustDelimiters@Filename@System@@AEAAXXZ")(&target);

    ReleaseString(&withExt);
    ReleaseString(&joined);
    ReleaseString(&dirPart);
    ReleaseString(&namePart);

    // ---- 2. the two BMP headers -------------------------------------------
    //
    // 14-byte BITMAPFILEHEADER and 40-byte BITMAPINFOHEADER, both built by
    // hand. Retail zeroes the file header with three stores (8+4+2) and the
    // info header with a `memset(p, 0, 0x28)` call into the CRT at 0x1800881b0.
    uint8_t fileHeader[14];
    uint8_t infoHeader[40];
    std::memset(fileHeader, 0, sizeof(fileHeader));
    std::memset(infoHeader, 0, sizeof(infoHeader));

    // Row stride: 3 bytes per pixel, rounded DOWN to a multiple of 4 after
    // adding one pixel's worth — `LEA EAX,[RDI+1] / LEA R12D,[RAX+RAX*2] /
    // AND R12D,0xfffffffc`. All 32-bit, so it is written 32-bit here.
    const int32_t rowBytes =
        static_cast<int32_t>((3u * static_cast<uint32_t>(width + 1)) & 0xfffffffcu);

    const uint16_t kBM = 0x4d42;
    std::memcpy(fileHeader + 0, &kBM, 2);                       // bfType
    const int32_t fileSize = rowBytes * height + 0x36;
    std::memcpy(fileHeader + 2, &fileSize, 4);                  // bfSize
    const int32_t offBits = 0x36;
    std::memcpy(fileHeader + 10, &offBits, 4);                  // bfOffBits

    const int32_t biSize = 0x28;
    std::memcpy(infoHeader + 0, &biSize, 4);
    std::memcpy(infoHeader + 4, &width, 4);
    std::memcpy(infoHeader + 8, &height, 4);
    // Retail writes planes, bitCount and compression as ONE eight-byte store of
    // 0x0000000000180001 (0x1800f29d0). Split into its fields here: the bytes
    // are identical, and compression's zero half is already there from the
    // memset above.
    const uint16_t planes = 1, bitCount = 24;
    std::memcpy(infoHeader + 12, &planes, 2);
    std::memcpy(infoHeader + 14, &bitCount, 2);
    const int32_t pelsPerMeter = 0xec4;                          // 3780 == 96 DPI
    std::memcpy(infoHeader + 24, &pelsPerMeter, 4);
    std::memcpy(infoHeader + 28, &pelsPerMeter, 4);

    // ---- 3. open --------------------------------------------------------
    //
    // NOTE THE ARGUMENT: CreatePath is given `path`, the function's ORIGINAL
    // argument, not the `.bmp` target built above (0x1800f29e6 `MOV RDX,R14`).
    void** fsSlot = FileSystemSingletonSlot();
    void* fs = fsSlot ? *fsSlot : nullptr;
    RT_EXPORT_ONCE(CreatePathFn, "?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z")(
        fs, path);

    // The stream is a stack object retail constructs, uses and destroys in
    // place. Raw storage plus retail's own constructor, the technique
    // FileStream.h describes and saveload_meta.cpp already uses.
    alignas(8) uint8_t streamStorage[sizeof(RtIo::BinaryFileStream)];
    uint8_t* const stream = streamStorage;
    RT_EXPORT_ONCE(StreamCtorFn, "??0BinaryFileStream@IO@@QEAA@XZ")(stream);

    // Retail calls FileStream::Create, NOT BinaryFileStream::Create.
    bool ok = RT_EXPORT_ONCE(
        StreamCreateFn, "?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z")(
            stream, &target);

    if (ok) {
        // Both offsets are FileStream.h's, asserted there against three
        // independent bodies; nothing is re-derived here.
        void* const handle =
            *reinterpret_cast<void**>(stream + offsetof(RtIo::FileStream, file) +
                                      offsetof(RtIO::FileInfo, winHandle));
        int32_t* const cursor =
            reinterpret_cast<int32_t*>(stream + offsetof(RtIo::FileStream, file) +
                                       offsetof(RtIO::FileInfo, cursor));

        *cursor += static_cast<int32_t>(RtVideoWriteFileRaw(handle, fileHeader, 14));
        *cursor += static_cast<int32_t>(RtVideoWriteFileRaw(handle, infoHeader, 0x28));

        // ---- 4. the pixels, bottom-up, 32bpp -> 24bpp ----------------------
        rtcoll::ArrayHdr rowBuf = { nullptr, 0, 0 };
        ArraySetSizeByte(&rowBuf, rowBytes, 0);

        // The padding bytes at the end of each row are written ONCE, by the
        // zero fill above, and never touched by the copy loop — so every row
        // carries the same zero padding. That is retail's behaviour and it is
        // why the buffer is filled rather than merely allocated.
        int32_t row = height - 1;
        if (row >= 0) {
            // Retail computes the first row's offset with a 32-BIT multiply and
            // only then sign-extends (`IMUL EAX,EDI / CDQE`), so the overflow
            // behaviour on a very large image is 32-bit. Reproduced rather than
            // widened.
            const int64_t rowStep = static_cast<int64_t>(-width) * 4;
            const uint8_t* src = reinterpret_cast<const uint8_t*>(pixels) + 2 +
                                 static_cast<int64_t>(row * width) * 4;

            for (; row >= 0; --row) {
                if (width > 0) {
                    uint8_t* dst = rowBuf._data;
                    const uint8_t* s = src;
                    for (int32_t i = 0; i < width; ++i) {
                        // src points one past the pixel's third byte, so these
                        // three reads are pixel bytes 0, 1, 2 in order — the
                        // low 24 bits of each 32-bit pixel, copied unswapped.
                        dst[0] = s[-2];
                        dst[1] = s[-1];
                        dst[2] = s[0];
                        s   += 4;
                        dst += 3;
                    }
                }
                *cursor += static_cast<int32_t>(
                    RtVideoWriteFileRaw(handle, rowBuf._data,
                                        static_cast<uint32_t>(rowBytes)));
                src += rowStep;
            }
        }

        // ---- 5. close, then release the row buffer ------------------------
        //
        // Retail closes the file HERE and the destructor below re-tests the
        // same handle field. Both are reproduced: CloseFile resets it to the
        // -1 sentinel, which is what stops the second close.
        int32_t* const fileHandle =
            reinterpret_cast<int32_t*>(stream + offsetof(RtIo::FileStream, file));
        if (*fileHandle != -1) {
            RT_EXPORT_ONCE(CloseFileFn,
                           "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z")(
                fs, fileHandle);
        }
        ok = true;   // 0x1800f2b23 `MOV DIL,1`, after the close, unconditionally

        if (rowBuf._data) rtcoll::MemFree(rowBuf._data);
        rowBuf._data = nullptr;
        rowBuf._size = 0;
        rowBuf._capacity = 0;
    }

    // ---- 6. destroy the stream, then the Filename -------------------------
    //
    // Reached from BOTH paths: the `Create` failure arm jumps straight here
    // (0x1800f2a13).
    //
    // *** RETAIL INLINES THE DESTRUCTOR; THIS CALLS THE EXPORTED ONE, AND THE
    // TWO WERE COMPARED INSTRUCTION BY INSTRUCTION BEFORE THE SUBSTITUTION. ***
    // Porting a destructor by assuming an exported body matches an inlined one
    // is how a prior lane nearly shipped a leak under a byte-identity claim
    // (`reference_a_written_recipe_can_be_wrong.md`), so the check came first:
    //
    //   ??1BinaryFileStream@IO@@UEAA@XZ (0x180023c80) is five instructions and
    //   tail-jumps to ??1FileStream@IO@@UEAA@XZ (0x1800aaab0), which does, in
    //   order: stamp ??_7FileStream (0x180154960); CloseFile if the handle is
    //   not -1; ~FileInfo(this+0x20) — releasing the Strings at FileInfo +0x28
    //   then +0x20; release `filename` at +0x18; stamp ??_7Stream
    //   (0x1801549a0); free the Encoding at +0x10 through 0x180006a10, which is
    //   `if (p) { Enter; Free(_singleton, p); Leave; }`.
    //
    //   The inlined copy at 0x1800f2b6f does the same six steps in the same
    //   order on the same offsets — stream+0x48 and +0x40 are exactly
    //   FileInfo's two Strings, and its encoding free is that same
    //   Enter/Free/Leave written out. The only difference is that the inlined
    //   copy stamps FileStream's table directly instead of arriving there
    //   through the thunk, which is the same store.
    RT_EXPORT_ONCE(StreamDtorFn, "??1BinaryFileStream@IO@@UEAA@XZ")(stream);
    ReleaseString(&target);

    return ok;
}

// ============================================================================
// Registration — BY NAME AND BY ADDRESS
// ============================================================================
int RtVideoSaveImageRegister() {
    RegisterRuntimeExport(
        "?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z",
        (void*)&Video_ImageBuffer_SaveImageToFile);
    RegisterRuntimeAddr(0x1800f2840, (void*)&Video_ImageBuffer_SaveImageToFile);
    printf("[RtPort] Video::ImageBuffer::SaveImageToFile (Wave 18 lane VRES): "
           "1 export registered\n");
    return 1;
}
