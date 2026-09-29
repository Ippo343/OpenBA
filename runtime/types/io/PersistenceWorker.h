#pragma once
// ============================================================================
// runtime/types/io/PersistenceWorker.h -- ENDGAME B.3.9 IO (W2)
// ============================================================================
//
// The save/load worker `IO::Persistence` embeds at +0x10: a `System::Thread`
// subclass whose table is the UNEXPORTED 2-slot vtable 0x180155860
// (slot 0 `Run` 0x1800b7260, slot 1 `System::Thread::OnCompleted`
// 0x18000c590). Retail never names the class -- no export, no RTTI string in
// the export directory -- so `IO::PersistenceWorker` is this project's name
// for it, and nothing mangles it (the table was minted BY ADDRESS, unexported
// row 1 of the B.3.9 inventory, and is retired).
//
// Construction: `??0Persistence@IO@@QEAA@XZ` (0x1800b8020) inlines this
// class's constructor into its own body --
//     LEA RBX,[RCX+0x10] ; MOV EDX,0x10000 ; CALL 0x1800ca4e0   ; ??0Thread(0x10000)
//     LEA RAX,[0x180155860] ; MOV [RBX],RAX                      ; this class's vptr
//     MOV qword [RBX+0x30],0 ; MOV qword [RBX+0x38],1 ; MOV dword [RBX+0x40],0
//     MOV qword [RBX+0x50..+0x78],0                               ; the three arrays
// -- i.e. the base's real ctor, the compiler's vptr store, then the member
// stores below. +0x44..+0x4f (fileHeader / magic / crc) are NOT written.
//
// Destruction: 0x1800b7740 (called directly by ??1Persistence 0x1800b7fb0)
// is `vptr = 0x180155860; ~Array x3; ~String; vptr = 0x180155f20; <four
// CloseHandles>` -- and the part after the second stamp is, instruction for
// instruction, `System::Thread::~Thread` (ReleaseFields: SetEvent(hStop),
// WaitForSingleObject(hThread, INFINITE), CloseHandle x4 in field order)
// inlined. So the class destructor is the first half, and the compiler's call
// into the base destructor -- with its own base-vptr store -- is the second:
// the by-address restamp of 0x180155f20 that persistence_worker_win.cpp used
// to spell is exactly that base-dtor restamp.
//
// FIELDS: every offset below is the `RtIo::Persistence` POD's minus 0x10, and
// is cross-asserted against it. B.4 R-IO: the jobs are members and read these
// fields by name. The String and the two arrays are real classes whose member
// destructors are the releases 0x1800b7740 made by hand (the destructor body no
// longer makes them, so nothing is released twice); the `IO::Persistence`
// class keeps its POD aliases of the same bytes, which it never destroys.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../system/Thread.h"
#include "../system/String.h"
#include "../collection/Collection.h"
#include "Persistence.h"

// The hand-derived Array row for `Array<SaveGameInfo>` (runtime/src/collection/
// array.cpp; 0x18002ae20 / 0x18002aae0 / 0x18002ac40 -- NOT trait-expressible,
// array_internal.h). Declared here so the member below can name its row.
namespace rtcoll {
extern const ArraySpec kSpec_SaveGameInfo;
}

namespace IO {

class Persistence;

class PersistenceWorker : public System::Thread {
public:
    // The inlined worker half of ??0Persistence 0x1800b8020 (see the banner).
    // The members' own ctors are retail's stores: String() nulls +0x30,
    // Array() zeroes each {data, size, capacity} triple.
    PersistenceWorker()
        : System::Thread(0x10000),
          _pendingFilename(), _status(1), _command(0), _blockCount(0),
          _blockDescs(), _blockPtrs(), _saves() {}

    // slot 0  +0x00  0x1800b7260 -- a five-way jump on `_command`; every arm a
    // tail JMP into a job body. Defined in persistence_worker_win.cpp.
    void Run() override;
    // slot 1  +0x08  0x18000c590 -- NOT overridden: System::Thread::OnCompleted.

    // ---- the five jobs `Run` tail-jumps into (all unexported) ------------
    void SaveJob();          // command 0  0x1800b6e00  persistence_worker_win.cpp
    void LoadJob();          // command 1  0x1800b6aa0  closure_batch2b_win.cpp
    void DeleteJob();        // command 2  0x1800b69c0  closure_batch2b_win.cpp
    void GetSaveInfoJob();   // command 3  0x1800b65b0  persistence_saveinfo_job.cpp
    void GetSaveListJob();   // command 4  0x1800b6290  persistence_savelist_job.cpp

    // B.4 R-IO: the String and the three arrays are the real classes. Their
    // member destructors ARE the four releases 0x1800b7740 makes: members die
    // in reverse declaration order, so _saves (+0x70, 0x18002ac40), _blockPtrs
    // (+0x60, 0x180024aa0), _blockDescs (+0x50, 0x18002a930), then
    // _pendingFilename (+0x30) -- retail's order.
    System::String                     _pendingFilename;  // +0x30  (Persistence +0x40)
    int32_t                            _status;           // +0x38  (+0x48)
    int32_t                            _command;          // +0x3C  (+0x4C)
    int32_t                            _blockCount;       // +0x40  (+0x50)
    uint32_t                           _fileHeader;       // +0x44  (+0x54) never written by the ctor
    uint32_t                           _magic;            // +0x48  (+0x58) never written by the ctor
    uint32_t                           _crc;              // +0x4C  (+0x5C) never written by the ctor
    Collection::Array<RtIo::BlockDesc> _blockDescs;       // +0x50  (+0x60)
    Collection::Array<void*>           _blockPtrs;        // +0x60  (+0x70)
    // The save list's element spec is the hand-derived row, named explicitly.
    Collection::Array<RtIo::SaveGameInfo, Collection::DefaultAllocator,
                      Collection::ExplicitSpec<&rtcoll::kSpec_SaveGameInfo>>
                                       _saves;            // +0x70  (+0x80)

protected:
    // 0x1800b7740's first half (see the banner): all four releases are the
    // members' destructors, so the body is empty. The compiler then runs
    // ~Thread. Non-virtual: the table has no dtor slot.
    // Out of line in persistence_worker_win.cpp.
    ~PersistenceWorker();

    friend class Persistence;   // ??1Persistence ends the embedded worker
};

static_assert(sizeof(PersistenceWorker) == 0x80, "the worker spans Persistence +0x10..+0x8F");
static_assert(offsetof(PersistenceWorker, _pendingFilename) == 0x30, "worker+0x30");
static_assert(offsetof(PersistenceWorker, _status)          == 0x38, "worker+0x38");
static_assert(offsetof(PersistenceWorker, _command)         == 0x3c, "Run 0x1800b7260: mov 0x3c(%rcx),%edx");
static_assert(offsetof(PersistenceWorker, _blockCount)      == 0x40, "worker+0x40");
static_assert(offsetof(PersistenceWorker, _fileHeader)      == 0x44, "SAVE job worker+0x44");
static_assert(offsetof(PersistenceWorker, _crc)             == 0x4c, "SAVE job worker+0x4c");
static_assert(offsetof(PersistenceWorker, _blockDescs)      == 0x50, "worker+0x50");
static_assert(offsetof(PersistenceWorker, _blockPtrs)       == 0x60, "worker+0x60");
static_assert(offsetof(PersistenceWorker, _saves)           == 0x70, "worker+0x70");

// ---- the cross-asserts against the Persistence POD (worker = POD + 0x10) ---
static_assert(offsetof(PersistenceWorker, _pendingFilename) + 0x10 ==
                  offsetof(RtIo::Persistence, pendingFilename), "alias pendingFilename");
static_assert(offsetof(PersistenceWorker, _status) + 0x10 ==
                  offsetof(RtIo::Persistence, status), "alias status");
static_assert(offsetof(PersistenceWorker, _command) + 0x10 ==
                  offsetof(RtIo::Persistence, command), "alias command");
static_assert(offsetof(PersistenceWorker, _blockCount) + 0x10 ==
                  offsetof(RtIo::Persistence, blockCount), "alias blockCount");
static_assert(offsetof(PersistenceWorker, _crc) + 0x10 ==
                  offsetof(RtIo::Persistence, crc), "alias crc");
static_assert(offsetof(PersistenceWorker, _blockDescs) + 0x10 ==
                  offsetof(RtIo::Persistence, blockDescData), "alias blockDescData");
static_assert(offsetof(PersistenceWorker, _blockPtrs) + 0x10 ==
                  offsetof(RtIo::Persistence, blockPtrData), "alias blockPtrData");
static_assert(offsetof(PersistenceWorker, _saves) + 0x10 ==
                  offsetof(RtIo::Persistence, saves), "alias saves");
static_assert(sizeof(PersistenceWorker) + 0x10 ==
                  offsetof(RtIo::Persistence, savePath), "the worker ends where _savePath starts");
static_assert(!std::has_virtual_destructor<PersistenceWorker>::value,
              "B.3.9 D1.3: non-virtual (the 2-slot table has no dtor slot)");

}  // namespace IO

// ==== B3R W0-P0c: Runtime bodies reached by retail address ====
// ENDGAME B.3R (B3R_RULES R5): each body below is registered at the retail
// address in its trailing comment and read through the registry today. It is
// declared here, copied from its definition (linkage, namespace, types), so the
// reader can call it by name. Nothing calls it through this declaration yet.
namespace System { class String; }     // system/String.h
namespace RtIo { struct SaveBlock; }    // io/Persistence.h

// (B.4 R-IO: the five jobs are PersistenceWorker members now, above.)
// ---- io/persistence_worker_win.cpp
extern "C" void RtIo_PersistenceWorker_EnqueueDelete(void* worker, const System::String* filename);  // 0x1800b7390
extern "C" void RtIo_PersistenceWorker_EnqueueLoad(void* worker, const System::String* filename, int blockCount, const RtIo::SaveBlock* blocks);  // 0x1800b7460
extern "C" void RtIo_PersistenceWorker_EnqueueSave(void* worker, const System::String* filename, int blockCount, const RtIo::SaveBlock* blocks);  // 0x1800b75c0
// ==== end B3R W0-P0c ====
