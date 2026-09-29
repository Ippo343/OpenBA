// ============================================================================
// runtime/src/io/closure_batch2b_win.cpp — cutover closure batch 2b
// ============================================================================
//
// The last two of IO::Persistence's embedded save/load worker jobs that our
// tree still reached by address:
//
//   0x1800b6aa0  IO_Persistence_Worker_Load     852 B   command 1
//   0x1800b69c0  IO_Persistence_Worker_Delete   214 B   command 2
//
// `runtime/src/io/persistence_worker_win.cpp` ported the dispatcher
// (`RtIo_PersistenceWorker_Run`, 0x1800b7260) and the three enqueue bodies and
// left all five job bodies as address doors; its own comment names these two as
// "the LOAD job" / "the DELETE job". They land here rather than in that file so
// the two commits stay separable, and the dispatcher needs no edit: it reaches
// them through `ResolveAddr(Module::Runtime, …)`, which consults the port
// registry first.
//
// WHY `_win`: both call Win32 directly — DeleteFileW in one, SetFilePointer in
// the other — exactly as persistence_worker_win.cpp's four bodies do. There is
// no platform-neutral half worth splitting out (PLAN.md §4.1).
//
// THE RECEIVER IS THE WORKER, `Persistence + 0x10`. Same subobject convention
// the sibling file documents at length. B.4 R-IO: both jobs are
// `IO::PersistenceWorker` members and read the worker's own fields by name
// (PersistenceWorker.h cross-asserts them against Persistence.h).
//
// ORACLE: none, and the reason is the sibling file's, unchanged. Neither body
// is pure; the LOAD job opens a file and fills caller-owned buffers, the DELETE
// job deletes a file. Running the shipped body "for comparison" would delete
// the save twice or read it twice into the same buffers. No RT_DIFF site is
// added that could only ever print `divergences=0`
// (reference_shadow_marker_is_not_an_oracle.md).
// ============================================================================

#include <cstdint>
#include <new>

#include <windows.h>

#include "../../types/io/Persistence.h"
#include "../../types/io/PersistenceWorker.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"
#include "../system/string.h"
#include "../../types/system/MemoryAllocator.h"
#include "../../types/system/WideString.h"

using RtIO::FileInfo;

namespace {

// ---- System::WideString, the inlined idiom -----------------------------------
//
// Retail inlines this; it is written out rather than routed through a ~String
// call because the ASYMMETRY is observable: the freeing branch leaves the slot
// holding `str-8` and the surviving branch nulls it. Same shape
// runtime/src/collection/array_destroy.cpp's Body B documents. WideString is
// not a B.3.1 class, so this stays hand-written C (ENDGAME B.3.1 step 2: the
// String/Filename equivalents, `AdoptString`/`ReleaseString`, are gone -- both
// call sites below are now `System::Filename`, whose ctor/dtor are that pair).
struct WideStr { wchar_t* data; };

// `??0WideString@System@@QEAA@AEBVString@1@@Z` is constructed straight into
// the WideStr slot (B3R RT-IO: by name, it was a door).
inline void WideRelease(WideStr* w) {
    if (!w->data) return;
    int32_t* rc = reinterpret_cast<int32_t*>(w->data) - 1;
    --*rc;
    if (*rc == 0) {
        w->data = reinterpret_cast<wchar_t*>(reinterpret_cast<uint8_t*>(w->data) - 8);
        ::System::MemoryAllocator::MemFree(w->data);   // 0x1800c7ad0
    } else {
        w->data = nullptr;
    }
}


}  // namespace

// ============================================================================
// 0x1800b69c0 — command 2, DELETE.
//
//     saved = FileSystem::_singleton[0x19];   (NOT touched by this job — see
//                                              the LOAD job, which does)
//     Filename f = persistence->pendingFilename;   (adopt + INC [str-4])
//     f.AdjustDelimiters();
//     WideString w(f);
//     BOOL ok = DeleteFileW(w);
//     release w; release f;
//     persistence->status = (ok != 1) + 1;    // 1 == deleted, 2 == failed
//
// That last line is retail's arithmetic verbatim (`SETNE`/`ADD`), not an
// if/else: the status codes are the same two values `?GetStatus@Persistence@IO@@`
// reports for every other job.
// ============================================================================
void IO::PersistenceWorker::DeleteJob() {
    // ENDGAME B.3.1 step 2: Adopt+AdjustDelimiters on a freshly-nulled dst is
    // Filename(const String&), retail 0x18000c780.
    System::Filename f(_pendingFilename);

    WideStr w;
    ::new (static_cast<void*>(&w)) System::WideString(f.AsString());   // B.3.x bridge

    const BOOL ok = DeleteFileW(w.data);

    WideRelease(&w);
    // ~f is retail's release of the adopted Filename.

    _status = static_cast<int32_t>((ok != 1) + 1);
}

// ============================================================================
// 0x1800b6aa0 — command 1, LOAD.
//
//     saved = FileSystem::_singleton->platformExistsGate;   // +0x19
//     FileSystem::_singleton->platformExistsGate = 1;
//     IO::FileStream st;                                    // stack, 0x50 B
//     Filename f = persistence->pendingFilename;
//     f.AdjustDelimiters();
//     if (st.Open(f)) {
//         for (i = 0; i < persistence->blockDescCount; ++i) {
//             void* dst = persistence->blockPtrData[i];
//             if (!dst) continue;                       // an absent block is OK
//             off = blockDesc[i].offset;                // stride 0xc, +0x04
//             st.file.cursor = off;
//             if ((uint32)st.file.handle > 0xfffffffd &&
//                 SetFilePointer(st.file.winHandle, off, 0, FILE_BEGIN) != off)
//                     goto fail;
//             if (!FileSystem::ReadFile(_singleton, &st.file,
//                                       blockDesc[i].size, dst)) goto fail;
//         }
//         if (st.file.handle != -1) FileSystem::CloseFile(_singleton, &st.file);
//         persistence->status = 1;
//     } else {
//   fail: persistence->status = 2;
//     }
//     ~st;
//     FileSystem::_singleton->platformExistsGate = saved;
//
// THREE THINGS THAT LOOK WRONG AND ARE NOT:
//
//  1. `st.file.cursor = off` is written BEFORE the seek, and it is written even
//     on the path that does not seek. That is deliberate on retail's part: for
//     a PACKED file there is no Win32 handle to seek and `FileSystem::ReadFile`
//     reads from `cursor` (+ the pack entry's baseOffset), so the store IS the
//     seek. The `handle > 0xfffffffd` test — unsigned, so it selects handle ==
//     -1 or -2, i.e. "not a packed file" (FileSystem.h's encoding note) — is
//     what adds the real seek for a loose file.
//  2. The gate at `_singleton+0x19` is set to 1 for the whole job and RESTORED
//     at the end, on every path. It is `Exists()`'s platform gate; forcing it
//     makes the load see loose files. Saved and restored around the destructor
//     too, exactly as retail does.
//  3. `dst == nullptr` SKIPS the block without failing. A caller that did not
//     supply a buffer for a block gets that block silently not loaded, and the
//     job still reports success. Faithful.
//
// THE FileStream IS CONSTRUCTED AND DESTROYED THROUGH ITS EXPORTED ctor/dtor
// rather than by transcribing retail's inlined copies. That is the same
// deliberate departure runtime/src/object/deleting_dtor.cpp makes and states:
// what is computed is identical (the inlined ctor IS ??0FileStream's body, and
// the epilogue IS ??1FileStream's), and the cost is two calls retail folds in.
// Reproducing the inlined destructor by hand would mean a second, competing
// copy of ~FileStream's String/FileInfo teardown in this tree.
// ============================================================================
void IO::PersistenceWorker::LoadJob() {
    ::IO::FileSystem* const fs = ::IO::FileSystem::_singleton;   // the class static
    const uint8_t savedGate = fs->_platformExistsGate;           // +0x19
    fs->_platformExistsGate = 1;

    {
        // ENDGAME B.3.9 IO / B.4 R-IO: the class, on the stack; its
        // destructor at the close of this block is ??1FileStream.
        IO::FileStream st;

        bool opened;
        {
            // Filename(const String&), 0x18000c780, as the DELETE job above.
            System::Filename f(_pendingFilename);
            opened = st.IO::FileStream::Open(f);
            // ~f is retail's release, right after Open returns.
        }

        if (opened) {
            bool failed = false;
            const RtIo::BlockDesc* const desc = _blockDescs._data;
            void* const* const           ptrs = _blockPtrs._data;
            for (int32_t i = 0; i < _blockDescs._size; ++i) {
                void* dst = ptrs[i];
                if (dst != nullptr) {
                    const uint32_t off  = desc[i].offset;                      // +0x04
                    const int32_t  size = static_cast<int32_t>(desc[i].size);  // +0x08
                    st._file.cursor = static_cast<int32_t>(off);
                    if (static_cast<uint32_t>(st._file.handle) > 0xfffffffdu &&
                        SetFilePointer(st._file.winHandle,
                                       static_cast<LONG>(off), nullptr, FILE_BEGIN) != off) {
                        failed = true;
                        break;
                    }
                    if (!fs->ReadFile(st._file, size, dst)) {
                        failed = true;
                        break;
                    }
                }
            }
            if (failed) {
                _status = 2;
            } else {
                if (st._file.handle != -1)
                    fs->CloseFile(st._file);
                _status = 1;
            }
        } else {
            _status = 2;
        }
    }

    fs->_platformExistsGate = savedGate;
}

