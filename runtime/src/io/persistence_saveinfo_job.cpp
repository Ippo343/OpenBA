// ============================================================================
// runtime/src/io/persistence_saveinfo_job.cpp — BATCH 8b (2026-09-12)
//   IO::Persistence's worker, COMMAND 3 — the GET-SAVE-INFO job.
//   retail 0x1800b65b0, UNEXPORTED, 1034 bytes / 258 instructions.
// ============================================================================
//
// The fourth of the five address doors `persistence_worker_win.cpp` opened when
// it ported the worker's `Run` dispatcher (0x1800b7260). BATCH 5 paid
// GET-SAVE-LIST (0x1800b6290, persistence_savelist_job.cpp); this pays
// GET-SAVE-INFO. LOAD (0x1800b6aa0) and DELETE (0x1800b69c0) remain.
//
// "Unexported" is MEASURED, not read off a mangling: the retail export
// directory was walked with pefile for this address before the file was
// written and returned nothing (reference_aeaa_is_private_not_unexported.md).
// So this is an ADDRESS row, registered with `RegisterRuntimeAddr` only, and
// renamed in retail Ghidra to `IO_Persistence_Worker_GetSaveInfoJob`.
//
// ---------------------------------------------------------------------------
// WHY BATCH 8 DECLINED IT, AND WHAT WAS ACTUALLY MISSING
// ---------------------------------------------------------------------------
// BATCH 8's report held this row back needing "FileStream::Open verification,
// an Array<{u32,u32,u32}>::SetSize port, and FileSystem+0x19 flag semantics".
// All three were already in the tree and none of them needed new work:
//
//   * `?Open@FileStream@IO@@UEAA_NAEBVFilename@System@@@Z` is EXPORTED
//     (ordinal 3304) and file_stream.cpp:1270 has ported it since lane FILE;
//     it is registered at both its name and 0x1800aac70.
//   * the Array is `kSpec_Blk12` — array.cpp:1191 has owned 0x18002a810 since
//     WAVE 25 lane INTERNAL42, under the name "the Persistence block
//     directory", which is exactly this array.
//   * `FileSystem+0x19` is `platformExistsGate`, named and static_asserted in
//     runtime/types/io/FileSystem.h since lane FIND. This body saves it, forces
//     it to 1 for the duration and restores it on every exit path (including
//     the early "could not open" one) — a scoped override, not a semantic
//     question.
//
// Nothing is ported here that another file already owned. The one door this
// body opens and does not close is `?Compute@CRC@System@@SAIPEBXHI@Z`
// (0x1800c5490, ordinal 1752) — see its call site below.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES — verify a save file's header and checksum without loading it
// ---------------------------------------------------------------------------
//   0. save `fs->platformExistsGate`, force it to 1.
//   1. open a stack BinaryFileStream on `pendingFilename` (AdjustDelimiters
//      first). Failure -> status = 2 (I/O) and straight to teardown.
//   2. read 4 bytes into `fileHeader`. Its HIGH 16 bits must equal
//      `blockCount` (what BeginGetSaveGameInfo asked for) or status = 3.
//   3. if the LOW 16 bits (the version) are 2, read 8 more bytes into
//      `magic`/`crc` and require magic == 0x80808080.
//   4. the version must be 1 or 2 (`(uint16_t)(ver-1) <= 1`), else status = 3.
//   5. SetSize(blockDescData, blockCount) and read blockCount*0xc bytes of
//      descriptors.
//   6. the file's size must equal exactly
//        headerBytes(4 or 12) + blockCount*0xc + sum(desc[i].size)
//      or status = 3.
//   7. version 2 only: recompute the CRC-32 the SAVE job wrote —
//        crc = CRC::Compute(descriptors, blockDescCount*0xc, seed 0xFFFFFFFF)
//        then, per descriptor IN ORDER: seek to desc[i].offset and stream
//        desc[i].size bytes through a 0x8000-byte stack buffer, folding each
//        0x8000 slice and COMPLEMENTING the accumulator once per slice.
//      That is `Crc32Chunked(crc, ., size, 0x8000)` — the same chunk rule the
//      SAVE job's payload pass uses, which is what makes writer and reader
//      agree (persistence_worker_win.cpp:187). A mismatch is status = 3.
//   8. success: CloseFile, status = 1.
//   9. teardown (every path): close the handle if still open, destroy the
//      BinaryFileStream, restore `platformExistsGate`.
//
// STATUS CODES, from this body alone: 1 = ok, 2 = could not open / short read,
// 3 = the file is not a save of the requested shape. `?GetStatus@` (0x1800b7c40)
// is the reader.
//
// ---------------------------------------------------------------------------
// THE SEEK IS CONDITIONAL ON THE FILE BEING LOOSE, AND THE TEST LOOKS INVERTED
// ---------------------------------------------------------------------------
//   1800b683c: cmp dword [rsp+0x40],0xfffffffd    ; FileInfo::handle
//   1800b6841: jbe 1800b6854                      ; SKIP the seek
// `handle` is not a handle (FileSystem.h): -1 = nothing opened, -2 = a LOOSE
// file with the real Win32 HANDLE at +0x18, anything else = a PACKED entry
// encoded as (packIndex<<24)|fileIndex. Read UNSIGNED, only -1 and -2 are
// above 0xfffffffd, so the `SetFilePointer` runs exactly for the loose case —
// where there is a real HANDLE to move — and a packed entry just gets its
// `cursor` field set, which is all `FileSystem::ReadFile` needs. The write to
// `cursor` (`[rsp+0x4c]`) happens for BOTH.
//
// The `SetFilePointer` call leaves EDX UNSET on purpose: it is still holding
// `desc[i].offset` from the `MOV EDX,[R12+RAX+4]` two instructions earlier.
// That is the distance argument, and it is why the store to `cursor` and the
// seek use one value with no reload.
//
// THE ReadFile RETURN IS IGNORED inside the checksum loop (no `TEST AL,AL`
// after 0x1800b688f), unlike the three header reads. Reproduced: a short read
// there folds stale buffer bytes and fails the CRC compare, which is retail's
// answer to a truncated save.
//
// ---------------------------------------------------------------------------
// NO ORACLE — same refusal as its siblings. The body reads a real file through
// a real HANDLE and writes one field; there is nothing to replay it against
// that does not re-read the save. Its verdict is end to end: the load dialog
// either accepts a save or reports it corrupt.
//
// PORTABILITY: `_win` because of the direct `SetFilePointer`, exactly like
// persistence_worker_win.cpp's direct `WriteFile`.
// ============================================================================

#include <windows.h>

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/Persistence.h"
#include "../../types/system/Filename.h"

using RtIo::BinaryFileStream;
using RtIo::Persistence;
using RtSystem::Filename;

// ---- cross-TU doors onto already-ported bodies, BY NAME NOT ADDRESS --------
// Declared locally rather than by pulling in each owner's header, the same
// technique persistence_worker_win.cpp uses for the identical set.
namespace RtSystem { void RtSystem_Filename_AdjustDelimiters(Filename* self); }
extern "C" bool RtIo_FileStream_Open(RtIo::FileStream* self, const Filename* path);
extern "C" BinaryFileStream* RtIo_BinaryFileStream_Ctor(BinaryFileStream* self);
extern "C" void              RtIo_BinaryFileStream_Dtor(BinaryFileStream* self);
extern "C" bool RtIo_FileSystem_ReadFile(RtIO::FileSystem* self, RtIO::FileInfo* fi,
                                         int32_t count, void* dst);
namespace RtIO {
    void RtIo_FileSystem_CloseFile(RtIO::FileSystem* self, RtIO::FileInfo* info);
}

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// One row of the block directory at Persistence+0x60 — stride 0xc, and the
// element `kSpec_Blk12` (array.cpp:1191) sizes. The SAVE job reads +0x8 as the
// payload length; this body reads +0x4 as the payload's file offset, which is
// what makes the seek above possible.
struct BlockDesc {
    uint32_t id;       // +0x00
    uint32_t offset;   // +0x04 — byte offset of the payload within the file
    uint32_t size;     // +0x08 — payload length
};
static_assert(sizeof(BlockDesc) == 0xC, "0x18002a810: LEA RCX,[RAX+RAX*2] / SHL RCX,2");
static_assert(offsetof(BlockDesc, offset) == 0x4, "0x1800b6833 MOV EDX,[R12+RAX+4]");
static_assert(offsetof(BlockDesc, size)   == 0x8, "0x1800b6857 MOV R14D,[R12+RAX+8]");

// `Collection::Array<BlockDesc>`, the three fields every Array in this engine
// has. Persistence.h models the same storage as two loose fields
// (blockDescData / blockDescCount); this view is what 0x18002a810 is handed.
struct BlockDescArray {
    BlockDesc* data;      // +0x00
    int32_t    size;      // +0x08
    int32_t    capacity;  // +0x0c
};
static_assert(offsetof(BlockDescArray, size) == 0x08, "Array::_size @ +0x08");

typedef void     (*ArraySetSizeFn)(void*, int);                     // 0x18002a810
typedef uint32_t (*CrcComputeFn)(const void*, int, uint32_t);       // 0x1800c5490

RtIO::FileSystem* Fs() {
    RtIO::FileSystem** slot = RT_DATA_SLOT(RtIO::FileSystem**,
                                           "?_singleton@FileSystem@IO@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

// ---- CRC-32, reflected table (polynomial 0xEDB88320) ------------------------
// System::CRC::_crcTable, retail 0x1801f9080, which 0x1800b689f loads by RIP.
// Generated rather than read out of the DLL so this body survives cutover
// (PLAN.md §4.1 decision 1); persistence_worker_win.cpp and type_system.cpp
// each carry their own generator for the same reason.
const uint32_t* CrcTable() {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    return table;
}

// Retail's inlined String release (0x1800b66b2). The freeing branch leaves the
// slot holding `data - 8`; only the still-referenced branch nulls it.
void ReleaseInline(char** slot) {
    char* p = *slot;
    if (!p) return;
    if (--reinterpret_cast<int32_t*>(p)[-1] == 0) {
        *slot = p - 8;
        using Fn = void (*)(void*);
        static Fn fn = nullptr;
        if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
        fn(p - 8);
    } else {
        *slot = nullptr;
    }
}

}  // namespace

// ============================================================================
// The worker's command-3 body.                              retail 0x1800b65b0
//
// `worker` is `Persistence + 0x10`; every offset in the shipped listing is
// `Persistence offset - 0x10` and this body uses the NAMED fields so a layout
// change breaks a static_assert rather than shifting a raw offset silently.
// ============================================================================
extern "C" void RtIo_PersistenceWorker_GetSaveInfo(void* worker) {
    Persistence* const p =
        reinterpret_cast<Persistence*>(reinterpret_cast<char*>(worker) - 0x10);

    RtIO::FileSystem* const fs = Fs();

    // ---- 0. scoped override of the Exists gate ----------------------------
    const uint8_t savedGate = fs->platformExistsGate;        // 0x1800b65e5
    fs->platformExistsGate = 1;                              // 0x1800b65f5

    BinaryFileStream bfs;
    RtIo_BinaryFileStream_Ctor(&bfs);                        // inlined 0x1800b6600

    // ---- 1. open pendingFilename ------------------------------------------
    // A raw refcount SHARE of the worker's String, not String::Set
    // (0x1800b666f: `MOV RAX,[RCX+0x30] / TEST / INC dword [RAX-4]`).
    Filename f;
    f.data = nullptr;
    if (p->pendingFilename.data != nullptr) {
        f.data = p->pendingFilename.data;
        ++reinterpret_cast<int32_t*>(f.data)[-1];
    }
    RtSystem::RtSystem_Filename_AdjustDelimiters(&f);        // 0x1800b6689

    const bool opened = RtIo_FileStream_Open(&bfs.base, &f); // 0x1800b669b
    ReleaseInline(&f.data);                                  // 0x1800b66a6

    if (!opened) {
        p->status = 2;                                       // 0x1800b66e0
    } else {
        // `headerBytes` is retail's ESI: 4 after the first read, 12 after the
        // optional second one, and it is the term the size check starts from.
        uint32_t headerBytes = 4;                            // 0x1800b66ed
        bool ioFail  = false;
        bool badFile = false;

        // ---- 2. the 4-byte file header ------------------------------------
        if (!RtIo_FileSystem_ReadFile(fs, &bfs.base.file, 4, &p->fileHeader)) {
            p->status = 2;
            ioFail = true;
        } else if (static_cast<uint32_t>(static_cast<uint16_t>(p->fileHeader >> 16)) !=
                   static_cast<uint32_t>(p->blockCount)) {    // 0x1800b6720
            badFile = true;
        } else {
            // ---- 3. version 2 carries {magic, crc} ------------------------
            if (static_cast<uint16_t>(p->fileHeader) == 2) {  // 0x1800b672a
                if (!RtIo_FileSystem_ReadFile(fs, &bfs.base.file, 8, &p->magic)) {
                    p->status = 2;
                    ioFail = true;
                } else {
                    headerBytes = 0xC;                        // 0x1800b675e
                    if (p->magic != 0x80808080u) badFile = true;
                }
            }

            // ---- 4. version must be 1 or 2 --------------------------------
            // `MOVZX EAX,WORD [R15+0x44] / DEC AX / CMP AX,1 / JA` — the
            // unsigned wrap is what rejects version 0.
            if (!ioFail && !badFile) {
                const uint16_t ver = static_cast<uint16_t>(p->fileHeader);
                if (static_cast<uint16_t>(ver - 1) > 1) badFile = true;  // 0x1800b6771
            }

            // ---- 5. the block directory -----------------------------------
            if (!ioFail && !badFile) {
                BlockDescArray* const descs =
                    reinterpret_cast<BlockDescArray*>(&p->blockDescData);
                {
                    static ArraySetSizeFn fn = nullptr;
                    if (!fn) fn = reinterpret_cast<ArraySetSizeFn>(
                        ResolveAddr(Module::Runtime, 0x18002a810));
                    fn(descs, static_cast<uint16_t>(p->fileHeader >> 16));  // 0x1800b678c
                }

                const int32_t n = descs->size;                // 0x1800b6791
                if (!RtIo_FileSystem_ReadFile(fs, &bfs.base.file,
                                              n * 0xC, descs->data)) {
                    p->status = 2;
                    ioFail = true;
                } else {
                    // ---- 6. the file size must add up ----------------------
                    // headerBytes + hdrBlockCount*0xc + sum(desc[i].size),
                    // where the middle term uses the count out of the HEADER
                    // and the sum uses the ARRAY's size. Retail reads the two
                    // from different places (0x1800b67c3 vs 0x1800b67d1) and
                    // SetSize has just made them equal.
                    uint32_t total = headerBytes +
                        static_cast<uint32_t>(
                            static_cast<uint16_t>(p->fileHeader >> 16)) * 0xCu;
                    for (int32_t i = 0; i < n; ++i)           // 0x1800b67e1
                        total += descs->data[i].size;

                    if (total != static_cast<uint32_t>(bfs.base.file.size)) {
                        badFile = true;                       // 0x1800b67f1
                    } else if (static_cast<uint16_t>(p->fileHeader) == 2) {
                        // ---- 7. recompute the checksum --------------------
                        // ADDRESS/NAME DOOR THIS BODY OPENS AND DOES NOT CLOSE:
                        //   SYMBOL      ?Compute@CRC@System@@SAIPEBXHI@Z
                        //               (ordinal 1752, retail 0x1800c5490)
                        //   CONDITION   every version-2 save-info request.
                        //   CONSEQUENCE at cutover it resolves NULL and this
                        //               call faults. It is a 10-instruction
                        //               leaf — the same running CRC-32 as the
                        //               fold below with one final complement —
                        //               and belongs to whoever takes the
                        //               System::CRC row, not to this file.
                        //   ADDRESSEE   the System namespace worklist.
                        uint32_t crc;
                        {
                            static void* cache = nullptr;
                            crc = reinterpret_cast<CrcComputeFn>(RtResolveCached(
                                "?Compute@CRC@System@@SAIPEBXHI@Z", &cache))(
                                    descs->data, n * 0xC, 0xFFFFFFFFu);   // 0x1800b6811
                        }

                        // The 0x8000-byte stack buffer at [RBP-0x80]; the
                        // frame is `_chkstk(0x8088)`, so this size is retail's.
                        static const uint32_t kChunk = 0x8000;
                        uint8_t buffer[0x8000];
                        const uint32_t* const table = CrcTable();

                        for (int32_t i = 0; i < descs->size; ++i) {  // 0x1800b6830
                            BlockDesc* const d = &descs->data[i];

                            bfs.base.file.cursor =
                                static_cast<int32_t>(d->offset);        // 0x1800b6838
                            // Loose (-1/-2) only; a packed entry needs no seek.
                            if (static_cast<uint32_t>(bfs.base.file.handle) > 0xFFFFFFFDu)
                                ::SetFilePointer(
                                    reinterpret_cast<HANDLE>(bfs.base.file.winHandle),
                                    static_cast<LONG>(d->offset), nullptr, FILE_BEGIN);

                            int32_t remaining = static_cast<int32_t>(d->size);
                            while (remaining > 0) {                     // 0x1800b6870
                                int32_t take = static_cast<int32_t>(kChunk);
                                if (remaining < take) take = remaining; // CMOVL
                                // The return value is IGNORED here; see banner.
                                RtIo_FileSystem_ReadFile(fs, &bfs.base.file,
                                                         take, buffer);
                                for (int32_t k = 0; k < take; ++k)      // 0x1800b68b0
                                    crc = table[(crc & 0xFFu) ^ buffer[k]] ^ (crc >> 8);
                                crc = ~crc;                             // 0x1800b68ce
                                remaining -= take;
                            }
                        }

                        if (crc != p->crc) badFile = true;              // 0x1800b68f3
                    }
                }
            }
        }

        if (!ioFail) {
            if (badFile) {
                p->status = 3;                                          // 0x1800b691b
            } else {
                // 0x1800b68f9 — retail's own close, BEFORE the teardown's.
                if (bfs.base.file.handle != -1)
                    RtIO::RtIo_FileSystem_CloseFile(fs, &bfs.base.file);
                p->status = 1;                                          // 0x1800b6911
            }
        }
    }

    // ---- 9. teardown, taken by EVERY path above ----------------------------
    // The failure-to-open arm jumps past the `LEA RSI,[0x1801549a0]` at
    // 0x1800b6923 only because RSI still holds that value from 0x1800b65f9;
    // the teardown itself is the same on both paths.
    if (bfs.base.file.handle != -1)
        RtIO::RtIo_FileSystem_CloseFile(fs, &bfs.base.file);            // 0x1800b6942
    RtIo_BinaryFileStream_Dtor(&bfs);                                   // 0x1800b694f..
    fs->platformExistsGate = savedGate;                                 // 0x1800b69a3
}

// ============================================================================
// Registration — ADDRESS ONLY (unexported; measured, not assumed).
// The address is a LITERAL so tools/runtime_census.py sees the door.
// ============================================================================
int RtIoPersistenceSaveInfoRegister() {
    RegisterRuntimeAddr(0x1800b65b0, (void*)&RtIo_PersistenceWorker_GetSaveInfo);
    return 1;
}
