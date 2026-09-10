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
// There is NO predicting gameplay route to this body, and the reason is worth
// writing down because it is not obvious. Ghidra lists three callers
// (UpdateList 0x1800c2227, UpdateLoadFromFile 0x1800c25d4, UpdateLoad
// 0x1800c29a6) and every one is an in-DLL rel32 — not a door. Our own ported
// copies of those three DO reach us, by name, through RT_EXPORT_ONCE — but
// only in LIVE mode: under BANISHED_RT_PREDICT=1 each of them SUPPRESSES the
// call (`if (out.Call(...)) setup(...)`) and hands the frame to the DLL, whose
// rel32 goes straight past us. So the standard "predicting round inside the
// registered entry" shape is structurally unreachable here, and shipping only
// that would have been a green line over a body no round ever entered
// (reference_uncalled_port_is_not_verified.md).
//
//   SCREEN A — the SYNTHESISED predicting sweep.  BANISHED_RT_SETUPLOAD_SWEEP=1
//   together with BANISHED_RT_PREDICT=1. Driven per frame from
//   standalone/saveload_harness.cpp. It builds SavedStates of its own, sweeps
//   the id space over EVERY id the live slot table carries plus ids that are
//   provably absent, and over both `_data == nullptr` and `_data != nullptr`,
//   and for each runs a real RtPredictRound: ours predicts and writes nothing,
//   the SHIPPED body runs for real, Verify() compares. Negative control is the
//   harness's own: BANISHED_RT_PREDICT_SELFTEST=1 corrupts one prediction per
//   round and the site must report it caught.
//
//   SCREEN B — the LIVE MIRROR.  BANISHED_RT_SETUPLOAD_MIRROR=1, predict OFF.
//   This is the screen that uses the game's REAL inputs. At each real entry it
//   runs the SHIPPED body first on a MIRROR SavedState carrying the same _id
//   and its own heap block, then runs ours for real on the true object, then
//   compares the two answers. A replay oracle is safe here only because the
//   mirror owns its own buffer — running the shipped body on the caller's own
//   SavedState after ours had already replaced _data would free a live block
//   twice. Pointer VALUES cannot be compared (two allocations), so the pointer
//   is checked STRUCTURALLY (ret->data is this->_data; both non-null).
//   Negative control: BANISHED_RT_SETUPLOAD_MIRROR=2 corrupts our _size after
//   the body and the screen must report it.
//
// Screen A can see the table walk and the field-by-field post-state but not
// the real ids; screen B sees the real ids but cannot compare the allocation.
// Neither is scored on the other's total.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saveload_internal.h"
#include "../../types/io/Persistence.h"
#include "../../funcs/rt_predict.h"

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
// The body, written ONCE against the sink so the live path and the predicting
// path cannot drift (rt_predict.h, "HOW A BODY SERVES BOTH MODES").
// ============================================================================
SaveBlock* RtSave_SavedState_SetupLoadBody(SavedState* self, SaveBlock* ret,
                                           RtSink& out, RtPredictRound* round) {
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
    // In predict mode this call is suppressed, so the pointer cannot be
    // predicted — the DLL's allocator picks it. Both slots that receive it are
    // declared out of model rather than compared against a null we invented.
    // This is a real hole and it is the reason screen B exists.
    if (round != nullptr) {
        round->Exclude(&self->_data, sizeof(void*),
                       "MemAlloc is suppressed in predict mode, so the address "
                       "the DLL's allocator returns is unpredictable — screen B "
                       "(the live mirror) is what checks this slot");
        round->Exclude(&ret->data, sizeof(void*),
                       "the same suppressed allocation, copied into the block");
    }
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

namespace {

// ---------------------------------------------------------------------------
// SCREEN B — the live mirror. 0 = off, 1 = on, 2 = on with the negative
// control armed (our _size is corrupted after the body runs and the screen must
// report it).
// ---------------------------------------------------------------------------
void MirrorSummaryAtExit();

int MirrorMode() {
    static int m = -1;
    if (m < 0) {
        const char* e = std::getenv("BANISHED_RT_SETUPLOAD_MIRROR");
        m = (e && e[0] && !(e[0] == '0' && e[1] == '\0')) ? std::atoi(e) : 0;
        if (m < 0) m = 0;
        // The tally has to reach the log even when the process ends by
        // self-quit: a screen whose numbers are never printed is not a screen.
        if (m > 0) std::atexit(&MirrorSummaryAtExit);
    }
    return m;
}

struct MirrorTotals {
    uint64_t calls = 0;   // real entries this screen scored
    uint64_t size  = 0;   // _size disagreed with the shipped body's
    uint64_t block = 0;   // the returned block disagreed
    uint64_t shape = 0;   // structural: ret->data is not this->_data, or null
    uint64_t control = 0; // negative-control rounds caught
    uint64_t controlRounds = 0;
    uint64_t changed = 0; // rounds where the shipped body actually moved _size
};
MirrorTotals g_mirror;

SaveBlock* SetupLoadMirrored(SavedState* self, SaveBlock* ret) {
    const uint32_t preSize = self->_size;

    // THEIRS first, on a mirror that owns its own buffer. Giving the mirror a
    // real block (not the caller's) is what makes the free arm run on both
    // sides without freeing one block twice.
    SavedState mirror;
    mirror._id   = self->_id;
    mirror._size = self->_size;
    mirror._data = (self->_data != nullptr)
                       ? MemAlloc(self->_size ? self->_size : 1u, 4)
                       : nullptr;

    SaveBlock theirs;
    std::memset(&theirs, 0xCD, sizeof(theirs));
    RT_ORIGINAL_ONCE(SetupLoadFn, kSetupLoadMangled)(&mirror, &theirs);

    // OURS, for real, on the true object.
    RtSink live;
    RtSave_SavedState_SetupLoadBody(self, ret, live, nullptr);

    if (MirrorMode() >= 2) {
        // Negative control: corrupt a value this screen actually COMPARES.
        self->_size += 1;
        g_mirror.controlRounds++;
    }

    g_mirror.calls++;
    if (mirror._size != preSize) g_mirror.changed++;

    bool bad = false;
    if (self->_size != mirror._size) {
        g_mirror.size++; bad = true;
        std::fprintf(stderr,
            "[SetupLoadMirror] SIZE MISMATCH id=%u  ours=%u theirs=%u (pre=%u)\n",
            self->_id, self->_size, mirror._size, preSize);
    }
    if (ret->size != theirs.size || ret->id != theirs.id) {
        g_mirror.block++; bad = true;
        std::fprintf(stderr,
            "[SetupLoadMirror] BLOCK MISMATCH ours{size=%u id=%u} "
            "theirs{size=%u id=%u}\n", ret->size, ret->id, theirs.size, theirs.id);
    }
    // The pointer VALUE cannot match (two allocations); the RELATION must.
    if (ret->data != self->_data || (theirs.data != nullptr) != (ret->data != nullptr)) {
        g_mirror.shape++; bad = true;
        std::fprintf(stderr,
            "[SetupLoadMirror] SHAPE MISMATCH ret->data=%p this->_data=%p "
            "theirs.data=%p\n", ret->data, self->_data, theirs.data);
    }
    if (bad && MirrorMode() >= 2) g_mirror.control++;

    MemFree(mirror._data);
    return ret;
}

}  // namespace

// ============================================================================
// The registered entry point.
//
// `this` in RCX, the hidden 0x10-byte return slot in RDX, the slot back in RAX.
// ============================================================================
SaveBlock* RtSave_SavedState_SetupLoad(SavedState* self, SaveBlock* ret) {
    RT_PREDICT_SITE("Save::SavedState::SetupLoad");
    if (RT_PREDICT_ON) {
        // Kept for completeness and for any caller that DOES reach us with the
        // oracle on. Under BANISHED_RT_PREDICT=1 none of the three real callers
        // does — see the banner — so screen A drives its own rounds instead of
        // relying on this one ever entering.
        RT_PREDICT_ROUND(round) {
            round.Watch(self, sizeof(SavedState), "Save::SavedState");
            round.Watch(ret, sizeof(SaveBlock), "IO::Persistence::SaveBlock [return slot]");
            RtSave_SavedState_SetupLoadBody(self, ret, round.sink(), &round);
            RT_ORIGINAL_ONCE(SetupLoadFn, kSetupLoadMangled)(self, ret);
        }
        return ret;
    }
    if (MirrorMode() > 0) return SetupLoadMirrored(self, ret);

    RtSink live;
    return RtSave_SavedState_SetupLoadBody(self, ret, live, nullptr);
}

// ============================================================================
// SCREEN A — the synthesised predicting sweep.
//
// Ticked per frame from standalone/saveload_harness.cpp. Does nothing at all
// unless BANISHED_RT_SETUPLOAD_SWEEP is set; warns once and does nothing if it
// is set without BANISHED_RT_PREDICT, because an RtPredictRound is inert when
// the oracle is off and a silent no-op reads exactly like a clean sweep.
// ============================================================================
extern "C" void RtSaveSetupLoadSweepTick(int frameNum) {
    static int  s_mode = -1;   // -1 unknown, 0 off, 1 armed
    static bool s_done = false;

    // The list drive must not fire before the save directory has something in
    // it: with no saves, UpdateList builds an empty array and walks straight to
    // state 5 without ever asking Persistence to read a header, so the slot
    // table stays empty and the sweep would only ever score the miss arm.
    // Default 600 sits after BANISHED_SAVE_TEST=400's ~461-frame completion.
    static int  s_kickFrame = 600;

    if (s_done) return;
    if (s_mode < 0) {
        const char* e = std::getenv("BANISHED_RT_SETUPLOAD_SWEEP");
        const bool sweepAsked = (e && e[0] && !(e[0] == '0' && e[1] == '\0'));
        if (e) { const int v = std::atoi(e); if (v > 1) s_kickFrame = v; }
        // The LIST drive below is what gets the three real callers to run at
        // all, so screen B needs this tick too even though it runs no rounds.
        s_mode = (sweepAsked || MirrorMode() > 0) ? 1 : 0;
        if (sweepAsked && !RtPredict::Enabled()) {
            std::fprintf(stderr,
                "[SetupLoadSweep] BANISHED_RT_SETUPLOAD_SWEEP is set but "
                "BANISHED_RT_PREDICT is NOT. An RtPredictRound is INERT with the "
                "oracle off, so no round below will compare anything. The list "
                "drive still runs (screen B needs it); the sweep does not.\n");
            std::fflush(stderr);
        }

    }
    if (s_mode == 0) return;

    const uint8_t* persistence = PersistenceSingleton();
    if (persistence == nullptr) return;
    const int32_t count = ((persistence[0x18] & 1) != 0)
                              ? 0
                              : *reinterpret_cast<const int32_t*>(persistence + 0x68);

    // Drive the LIST operation once. The slot table this body reads is filled
    // by the save-header read that UpdateList state 2 kicks off, so without a
    // list in flight the sweep can only ever see the empty-table early-out.
    // This is the same SetOperation the load dialog calls — a real path, not a
    // synthetic poke at engine state — and it is reached only under this
    // screen's own env var.
    if (count <= 0 && frameNum >= s_kickFrame) {
        static bool s_kicked = false;
        if (!s_kicked) {
            void** slot = reinterpret_cast<void**>(
                ResolveExport("?_singleton@SaveLoad@Save@@0PEAV12@EA"));
            using SetOpFn = void (*)(void*, int32_t, int32_t);
            if (slot && *slot) {
                s_kicked = true;
                std::fprintf(stderr,
                    "[SetupLoadSweep] frame %d: slot table empty -- driving "
                    "SetOperation(List, -1) to populate it\n", frameNum);
                std::fflush(stderr);
                RT_EXPORT_ONCE(SetOpFn,
                    "?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z")(
                        *slot, RtSave::kOpList, -1);
            }
        }
    }

    // Wait for the slot table to carry something, so the sweep exercises the
    // MATCH arm and not only the empty-table early-out. Give up after ~40s and
    // run anyway rather than reporting nothing at all — a sweep that covers
    // only the miss arm is still a sweep, and saying so is better than silence.
    if (count <= 0 && frameNum < s_kickFrame + 1200) return;

    s_done = true;
    std::fprintf(stderr, "[SetupLoadSweep] frame %d: slot table count=%d\n",
                 frameNum, count);

    const SlotRow* table =
        (count > 0) ? *reinterpret_cast<const SlotRow* const*>(persistence + 0x60)
                    : nullptr;

    // The id set: every id the table carries (the MATCH arm, with the real
    // sizes retail will hand back), plus ids proved absent by the same lookup
    // the body uses (the MISS arm). Absent ids are SEARCHED for rather than
    // assumed, so the miss arm is a fact about this table and not a guess.
    int32_t ids[32];
    int     idCount = 0;
    for (int32_t i = 0; i < count && idCount < 24; ++i) ids[idCount++] = table[i].id;

    int32_t probe = 0x7000000;
    while (idCount < 28 && probe < 0x7000100) {
        bool found = false;
        LookupSlotSize(persistence, probe, &found);
        if (!found) ids[idCount++] = probe;
        ++probe;
    }

    RT_PREDICT_SITE("Save::SavedState::SetupLoad [synthesised sweep]");
    uint64_t rounds = 0;
    for (int i = 0; i < idCount; ++i) {
        for (int withData = 0; withData < 2; ++withData) {
            SavedState st;
            st._id   = static_cast<uint32_t>(ids[i]);
            // A non-zero pre-size so the `predicted != pre` effectiveness
            // control can move even when the looked-up size is 0.
            st._size = 0xA5A5A5A5u;
            st._data = withData ? MemAlloc(64, 4) : nullptr;

            SaveBlock blk;
            std::memset(&blk, 0xCD, sizeof(blk));

            if (RT_PREDICT_ON) {
                RT_PREDICT_ROUND(round) {
                    round.Watch(&st, sizeof(st), "Save::SavedState [synthesised]");
                    round.Watch(&blk, sizeof(blk), "IO::Persistence::SaveBlock [return slot]");
                    RtSave_SavedState_SetupLoadBody(&st, &blk, round.sink(), &round);
                    RT_ORIGINAL_ONCE(SetupLoadFn, kSetupLoadMangled)(&st, &blk);
                }
                rounds++;
            }
            MemFree(st._data);
        }
    }
    std::fprintf(stderr,
        "[SetupLoadSweep] %llu rounds over %d ids x {no data, data} — verdicts "
        "are on the [synthesised sweep] row of the [RtPredict] summary\n",
        (unsigned long long)rounds, idCount);
    std::fflush(stderr);
}

// ============================================================================
// THE WAVE 13 OPEN SIGNAL — nine unpredicted bytes at SaveLoad+0x2c, state 4.
// (docs/runtime_port/wave13/OPEN_SIGNAL_saveload_0x2c.md)
//
// The integrator's hypothesis was the save/load WORKER THREAD filling the block
// asynchronously. Reading the disassembly says otherwise, and names a
// DETERMINISTIC, SINGLE-THREADED writer that runs inside state 4 itself:
//
//   ?LoadMetaData@SaveLoad@Save@@ (0x1800c1b30), which state 4 calls at
//   0x1800c22c6, ENDS by destroying the embedded SavedState's buffer:
//
//     1800c1d98  MOV RCX,[R14+0x30]        ; this->_embeddedState._data
//     1800c1d9c  TEST RCX,RCX / JZ
//     1800c1da1  CALL 0x1800c7ad0          ; MemFree
//     1800c1da6  MOV qword ptr [R14+0x30],0x0
//     1800c1dae  MOV dword ptr [R14+0x2c],0x0
//
//   (It only READS them, at 0x1800c1b88/0x1800c1b99, to open a MemoryStream
//   over the loaded bytes; the zeroing is the teardown.)
//
// That accounts for every property of the signal, including the one the doc
// correctly flagged as evidence AGAINST a thread race:
//   * the region is exactly +0x2c (4 bytes of _size) and +0x30 (8 of _data);
//   * it is state 4, not state 3;
//   * our body SUPPRESSES the LoadMetaData call in predict mode, so those
//     writes are never predicted while the DLL performs them for real;
//   * the count is CONSTANT at 9 because the post-values are all ZERO: 4 bytes
//     of _size plus the 5 low bytes of a user-mode heap pointer, whose top 3
//     bytes are already zero. A cross-thread write would not be so steady.
//
// This tick is the confirming measurement rather than the argument: it polls
// the SaveLoad singleton's embedded SavedState every frame and logs _progress,
// _size and _data whenever any of them move, so the VALUES either are zero
// after a state-4 round or they are not. Gated by BANISHED_RT_SETUPLOAD_WATCH.
// ============================================================================
extern "C" void RtSaveEmbeddedStateWatchTick(int frameNum) {
    static int s_on = -1;
    if (s_on < 0) {
        const char* e = std::getenv("BANISHED_RT_SETUPLOAD_WATCH");
        s_on = (e && e[0] && !(e[0] == '0' && e[1] == '\0')) ? 1 : 0;
    }
    if (s_on == 0) return;

    static void** slot = nullptr;
    if (!slot) slot = reinterpret_cast<void**>(
        ResolveExport("?_singleton@SaveLoad@Save@@0PEAV12@EA"));
    if (!slot || !*slot) return;

    const uint8_t* sl = static_cast<const uint8_t*>(*slot);
    const uint32_t progress = *reinterpret_cast<const uint32_t*>(sl + 0x18);
    const uint32_t size     = *reinterpret_cast<const uint32_t*>(sl + 0x2c);
    const void*    data     = *reinterpret_cast<void* const*>(sl + 0x30);

    static uint32_t lastProgress = 0xFFFFFFFFu;
    static uint32_t lastSize     = 0xFFFFFFFFu;
    static const void* lastData  = reinterpret_cast<const void*>(~static_cast<uintptr_t>(0));
    static int emitted = 0;
    if (progress == lastProgress && size == lastSize && data == lastData) return;
    lastProgress = progress; lastSize = size; lastData = data;
    if (emitted++ > 200) return;
    std::fprintf(stderr,
        "[EmbeddedState] frame %d  _progress=%u  +0x2c _size=%u  +0x30 _data=%p\n",
        frameNum, progress, size, data);
    std::fflush(stderr);
}

// Screen B's tally, printed at shutdown beside the [RtPredict] summary.
extern "C" void RtSaveSetupLoadMirrorSummary();
namespace { void MirrorSummaryAtExit() { RtSaveSetupLoadMirrorSummary(); } }

extern "C" void RtSaveSetupLoadMirrorSummary() {
    if (MirrorMode() == 0) return;
    std::fprintf(stderr,
        "[SetupLoadMirror] calls=%llu changed=%llu size-mismatch=%llu "
        "block-mismatch=%llu shape-mismatch=%llu | negative control: rounds=%llu "
        "caught=%llu\n",
        (unsigned long long)g_mirror.calls,
        (unsigned long long)g_mirror.changed,
        (unsigned long long)g_mirror.size,
        (unsigned long long)g_mirror.block,
        (unsigned long long)g_mirror.shape,
        (unsigned long long)g_mirror.controlRounds,
        (unsigned long long)g_mirror.control);
    if (g_mirror.calls == 0) {
        std::fprintf(stderr,
            "[SetupLoadMirror] calls=0 — this screen scored NOTHING. It is not a "
            "pass (reference_uncalled_port_is_not_verified.md).\n");
    }
    std::fflush(stderr);
}

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
