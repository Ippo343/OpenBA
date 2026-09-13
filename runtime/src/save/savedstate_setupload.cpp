// ============================================================================
// runtime/src/save/savedstate_setupload.cpp — Wave 14 lane SETUPLOAD
//
//   ?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ
//   retail Runtime-x64.dll 0x1800c0c60, ordinal 4031
//
// One of the last seven unported rows in the Save namespace. Its close sibling
// ?GetSaveBlock@SavedState@Save@@QEBA?AUSaveBlock@Persistence@IO@@XZ
// (0x1800c0d50) is READ here as a cross-reference and deliberately NOT ported:
// its eight instructions are byte for byte SetupLoad's tail, which is what
// pins the SaveBlock field order independently of UpdateSave.
//
// ----------------------------------------------------------------------------
// THE ABI, AND A CORRECTION TO THE LANE BRIEF
// ----------------------------------------------------------------------------
// The mangled return type is `?AUSaveBlock@Persistence@IO@@` — a struct BY
// VALUE, 0x10 bytes, so it is memory-returned and there is a hidden return
// slot. The lane brief said the real shape is
// `void f(SaveBlock* retslot, SavedState* self)`, i.e. the slot FIRST. That is
// the rule for a static/free function. For a NON-STATIC MEMBER MSVC keeps
// `this` in RCX and puts the hidden slot in RDX, and the disassembly says so
// with no ambiguity:
//
//   1800c0c7e  MOV R14,RDX          ; R14 = the slot; later written at +0/+8/+0xc
//   1800c0c81  MOV RSI,RCX          ; RSI = this;  read at [RSI] (_id), [RSI+8]
//   1800c0c84  MOV RCX,[RCX+0x8]    ;              (_data), written at [RSI+4]
//
// and the CALLER agrees (UpdateList 0x1800c221f: `ADD RCX,0x28` = the embedded
// SavedState, `LEA RDX,[RBP-0x61]` = the block). `tools/abi_retslot_audit.py`'s
// own docstring states the same rule ("RDX for a member, after `this` in RCX"),
// and `saveload_update.cpp`'s existing `SetupLoadFn` typedef already had it
// right. So: `SaveBlock* SetupLoad(SavedState* self, SaveBlock* ret)`, RAX =
// ret. Getting this backwards would have written 16 bytes through a stale RCX.
//
// ----------------------------------------------------------------------------
// WHAT THE BODY DOES — transcribed instruction for instruction
// ----------------------------------------------------------------------------
//   1. `if (this->_data) MemFree(this->_data);`
//      0x1800c0c84..0x1800c0c8d. The `TEST RCX,RCX / JZ` is REDUNDANT with the
//      callee's own first instruction (0x1800c7ad0 opens with `TEST RCX,RCX`),
//      and it is transcribed anyway because it is what the bytes do.
//      NOTE: _data is NOT nulled here. It is overwritten at step 4.
//
//   2. Look the save's byte count up in Persistence's SLOT TABLE.
//      0x1800c0c92..0x1800c0cd4. Reads the Persistence singleton at
//      *0x1801fc6b8, and:
//        * bails to size 0 when `persistence[0x18] & 1` is set — the same
//          "shutting down / no persistence" bit UpdateList, UpdateSave and
//          IO::Persistence::BeginLoad (0x1800b7992) all test first;
//        * otherwise walks `count = *(int32*)(persistence+0x68)` entries of
//          **12 bytes** starting at `*(void**)(persistence+0x60)`, matching
//          `entry.id == this->_id`, and takes `entry[+8]` as the size.
//        * NOT FOUND (or count <= 0) leaves the size at ZERO. There is no
//          error path and no assert: a SavedState whose id the table does not
//          carry gets a zero-byte buffer.
//      The 12-byte stride is not an inference: `ADD RCX,0xc` walks the keys and
//      `MOV EDX,[R10 + RCX*0x4 + 0x8]` with RCX = 3*index reads 12*index + 8.
//      The middle dword (+0x4) is never read by this function.
//
//      WHO WRITES THAT TABLE IS NOT ESTABLISHED BY THIS LANE, and the obvious
//      candidate is wrong: `IO::Persistence::BeginLoad` (0x1800b7970) calls
//      FUN_1800b7460 with `LEA RCX,[RBX+0x10]`, so the array FUN_1800b7460
//      clears at its own +0x60/+0x68 with an 8-byte stride is Persistence+0x70,
//      a DIFFERENT array. Do not read the two as one.
//
//   3. `this->_size = size;`  (0x1800c0cd4)
//
//   4. `this->_data = MemAlloc(size, 4);`  (0x1800c0cd9..0x1800c0d16)
//      Open-coded, not a call to the export: `EnterCriticalSection(
//      *0x1801fc718 + 0x640); Allocate(*0x1801fc718, size, 4);
//      LeaveCriticalSection(...)`. That triple IS
//      ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z at 0x1800c7a50 — compare
//      0x1800c7a6e..0x1800c7aa3, same four operands in the same order — so the
//      port goes through the one allocator door the rest of the tree uses
//      instead of pinning 0x1800c6e60 and the +0x640 offset a second time.
//      The allocation happens EVEN WHEN size IS ZERO. Faithful; do not "fix".
//
//   5. Fill the return slot: `ret->data = this->_data; ret->size = this->_size;
//      ret->id = this->_id;` (0x1800c0d1a..0x1800c0d26) and return it in RAX.
//      Note retail RE-READS `this->_size` from memory at 0x1800c0d1d rather
//      than reusing EBX, which is why the port reads it back through the sink.
//
// ----------------------------------------------------------------------------
// VERIFICATION — TWO SCREENS, SCORED SEPARATELY
// ----------------------------------------------------------------------------
// The body below is written against the live sink (runtime/funcs/rt_sink.h).
// Ghidra lists three callers (UpdateList 0x1800c2227, UpdateLoadFromFile
// 0x1800c25d4, UpdateLoad 0x1800c29a6); our own ported copies of those three
// reach us by name through RT_EXPORT_ONCE.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_sink.h"
#include "saveload_internal.h"
#include "../../types/io/Persistence.h"

using RtSave::SavedState;
using RtIo::SaveBlock;
using RtSaveDetail::MemAlloc;
using RtSaveDetail::MemFree;
using RtSaveDetail::PersistenceSingleton;

namespace {

using MemAllocFn = void* (*)(uint64_t, int);
using MemFreeFn  = void  (*)(void*);
// NOT minted here. `RtSaveDetail::SetupLoadFn` (saveload_internal.h) is the one
// definition of this shape, hoisted out of saveload_update.cpp's anonymous
// namespace by this lane precisely so that the port and its three callers
// cannot end up with two spellings of an ABI whose drift mode is a wild write.
using RtSaveDetail::SetupLoadFn;

const char* const kSetupLoadMangled =
    "?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ";

// The 12-byte row SetupLoad walks at Persistence+0x60, count at +0x68.
// Named for what THIS function uses it for; +0x4 is never read here and is not
// claimed to be padding.
struct SlotRow {
    int32_t  id;       // +0x00  matched against SavedState::_id
    int32_t  unread4;  // +0x04  NOT read by SetupLoad
    uint32_t size;     // +0x08  the byte count handed to MemAlloc
};
static_assert(sizeof(SlotRow) == 12, "Persistence slot row is 12 bytes (ADD RCX,0xc)");

const void* MemAllocTarget() {
    static const void* p = reinterpret_cast<const void*>(
        RT_EXPORT_ONCE(MemAllocFn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    return p;
}
const void* MemFreeTarget() {
    static const void* p = reinterpret_cast<const void*>(
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    return p;
}

// ---------------------------------------------------------------------------
// Step 2 on its own, because both the body and screen A's driver need it and a
// second transcription is a second thing to drift.
//
// `found` is an OUT parameter and not a sentinel: a row whose size is genuinely
// 0 and a missing row both yield 0, and the driver has to be able to tell them
// apart to choose its absent-id probes.
// ---------------------------------------------------------------------------
uint32_t LookupSlotSize(const uint8_t* persistence, int32_t id, bool* found) {
    if (found) *found = false;
    if (persistence == nullptr) return 0;
    if ((persistence[0x18] & 1) != 0) return 0;                 // 0x1800c0c9b
    const int32_t count = *reinterpret_cast<const int32_t*>(persistence + 0x68);
    if (count <= 0) return 0;                                    // 0x1800c0ca8
    const SlotRow* table =
        *reinterpret_cast<const SlotRow* const*>(persistence + 0x60);
    for (int32_t i = 0; i < count; ++i) {                        // 0x1800c0cb7
        if (table[i].id == id) {
            if (found) *found = true;
            return table[i].size;                                // 0x1800c0ccf
        }
    }
    return 0;                                                    // EDX stays 0
}

}  // namespace

// ============================================================================
// The body, written against the live sink (rt_sink.h).
// ============================================================================
SaveBlock* RtSave_SavedState_SetupLoadBody(SavedState* self, SaveBlock* ret,
                                           RtSink& out) {
    // ---- 1. free the previous buffer ---------------------------------------
    void* old = out.ReadPtr(&self->_data);
    if (old != nullptr) {
        if (out.Call("System::MemoryAllocator::MemFree(this->_data)",
                     MemFreeTarget(), reinterpret_cast<int64_t>(old))) {
            MemFree(old);
        }
    }

    // ---- 2. the slot-table lookup ------------------------------------------
    const uint8_t* persistence = PersistenceSingleton();
    const uint32_t size = LookupSlotSize(persistence,
                                         static_cast<int32_t>(out.ReadU32(&self->_id)),
                                         nullptr);

    // ---- 3. publish the size ------------------------------------------------
    out.U32(&self->_size, size,
            "SavedState::_size = Persistence slot-table size (0 when absent)");

    // ---- 4. the fresh buffer ------------------------------------------------
    void* buf = nullptr;
    if (out.Call("System::MemoryAllocator::MemAlloc(this->_size, 4)",
                 MemAllocTarget(), static_cast<int64_t>(size), 4)) {
        buf = MemAlloc(size, 4);
    }
    out.Ptr(&self->_data, buf, "SavedState::_data = MemAlloc(_size, 4)");

    // ---- 5. fill the return slot -------------------------------------------
    // Retail re-reads _size and _id from `this` here rather than reusing the
    // registers it already has, so these go through the sink's read-back.
    out.Ptr(&ret->data, buf, "SaveBlock::data = this->_data");
    out.U32(&ret->size, out.ReadU32(&self->_size), "SaveBlock::size = this->_size");
    out.U32(&ret->id,   out.ReadU32(&self->_id),   "SaveBlock::id = this->_id");
    return ret;
}

// ============================================================================
// The registered entry point.
//
// `this` in RCX, the hidden 0x10-byte return slot in RDX, the slot back in RAX.
// ============================================================================
SaveBlock* RtSave_SavedState_SetupLoad(SavedState* self, SaveBlock* ret) {
    RtSink live;
    return RtSave_SavedState_SetupLoadBody(self, ret, live);
}

// ============================================================================
// A3-HOLD: the two per-frame ticks were the screens of a DLL-vs-port oracle.
// With no DLL they can never compare anything again, so the machinery is gone;
// these two entry points stay as no-ops only because
// standalone/saveload_harness.cpp still calls them.
// ============================================================================
extern "C" void RtSaveSetupLoadSweepTick(int) {}
extern "C" void RtSaveEmbeddedStateWatchTick(int) {}

// ============================================================================
// Registration — BOTH doors.
//
// The name door is what the census scanner sees and what our own ported
// UpdateList / UpdateLoad / UpdateLoadFromFile resolve through; the address
// door is what a by-address caller and the cutover boot need. The mangled name
// below was checked byte for byte against the shipped export table before it
// was written (reference_resolveexport_phantom_names.md), and `QEAA` here means
// PUBLIC non-virtual, not "unexported"
// (reference_aeaa_is_private_not_unexported.md).
//
// Neither door existed before this lane: `SetupLoad` and `0x1800c0c60` were
// grepped across runtime/, code/, engine/ and standalone/ first, and the only
// hits were saveload_update.cpp's three RT_EXPORT_ONCE *call sites* and the
// WAVE9 planning doc — no registration, so this is not a duplicate
// (reference_grep_for_the_port_before_writing_it.md).
// ============================================================================
int RtSaveSetupLoadRegister() {
    RegisterRuntimeExport(
        "?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ",
        (void*)&RtSave_SavedState_SetupLoad);
    RegisterRuntimeAddr(0x1800c0c60, (void*)&RtSave_SavedState_SetupLoad);
    return 1;
}
