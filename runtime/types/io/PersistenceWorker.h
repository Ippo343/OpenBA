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
// is cross-asserted against it; the POD (and the `IO::Persistence` class) stay
// the views the job bodies and the enqueue helpers read through. The String
// stays a POD for the reason Persistence.h gives: the destructor releases it
// BY HAND, and a `System::String` member would release it a second time.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../system/Thread.h"
#include "Persistence.h"

namespace IO {

class Persistence;

class PersistenceWorker : public System::Thread {
public:
    // The inlined worker half of ??0Persistence 0x1800b8020 (see the banner).
    PersistenceWorker()
        : System::Thread(0x10000),
          _pendingFilename{nullptr}, _status(1), _command(0), _blockCount(0),
          _blockDescData(nullptr), _blockDescCount(0), _pad5c(0),
          _blockPtrData(nullptr), _blockPtrCount(0), _pad6c(0),
          _saves(nullptr), _saveCount(0), _pad7c(0) {}

    // slot 0  +0x00  0x1800b7260 -- a five-way jump on `_command`; every arm a
    // tail JMP into a job body. Defined in persistence_worker_win.cpp.
    void Run() override;
    // slot 1  +0x08  0x18000c590 -- NOT overridden: System::Thread::OnCompleted.

    RtSystem::String    _pendingFilename;   // +0x30  (Persistence +0x40)
    int32_t             _status;            // +0x38  (+0x48)
    int32_t             _command;           // +0x3C  (+0x4C)
    int32_t             _blockCount;        // +0x40  (+0x50)
    uint32_t            _fileHeader;        // +0x44  (+0x54) never written by the ctor
    uint32_t            _magic;             // +0x48  (+0x58) never written by the ctor
    uint32_t            _crc;               // +0x4C  (+0x5C) never written by the ctor
    void*               _blockDescData;     // +0x50  (+0x60) Array<{id,?,size}>
    int32_t             _blockDescCount;    // +0x58  (+0x68)
    int32_t             _pad5c;             // +0x5C  (+0x6C) capacity; the ctor's qword store zeroes it
    void*               _blockPtrData;      // +0x60  (+0x70) Array<void*>
    int32_t             _blockPtrCount;     // +0x68  (+0x78)
    int32_t             _pad6c;             // +0x6C  (+0x7C)
    RtIo::SaveGameInfo* _saves;             // +0x70  (+0x80)
    int32_t             _saveCount;         // +0x78  (+0x88)
    int32_t             _pad7c;             // +0x7C  (+0x8C)

protected:
    // 0x1800b7740's first half (see the banner): the three array destructors
    // (+0x70, +0x60, +0x50 -- retail's order) and the String release. The
    // compiler then runs ~Thread. Non-virtual: the table has no dtor slot.
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
static_assert(offsetof(PersistenceWorker, _blockDescData)   == 0x50, "worker+0x50");
static_assert(offsetof(PersistenceWorker, _blockPtrData)    == 0x60, "worker+0x60");
static_assert(offsetof(PersistenceWorker, _saves)           == 0x70, "worker+0x70");
static_assert(offsetof(PersistenceWorker, _saveCount)       == 0x78, "worker+0x78");

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
static_assert(offsetof(PersistenceWorker, _blockDescData) + 0x10 ==
                  offsetof(RtIo::Persistence, blockDescData), "alias blockDescData");
static_assert(offsetof(PersistenceWorker, _blockPtrData) + 0x10 ==
                  offsetof(RtIo::Persistence, blockPtrData), "alias blockPtrData");
static_assert(offsetof(PersistenceWorker, _saves) + 0x10 ==
                  offsetof(RtIo::Persistence, saves), "alias saves");
static_assert(offsetof(PersistenceWorker, _saveCount) + 0x10 ==
                  offsetof(RtIo::Persistence, saveCount), "alias saveCount");
static_assert(sizeof(PersistenceWorker) + 0x10 ==
                  offsetof(RtIo::Persistence, savePath), "the worker ends where _savePath starts");
static_assert(!std::has_virtual_destructor<PersistenceWorker>::value,
              "B.3.9 D1.3: non-virtual (the 2-slot table has no dtor slot)");

}  // namespace IO
