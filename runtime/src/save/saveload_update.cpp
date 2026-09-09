// ============================================================================
// runtime/src/save/saveload_update.cpp — Wave 13 — the SaveLoad state machines
// ============================================================================
//
// Wave 5 lane C ported 16 of Save::SaveLoad's exports and left the five
// UpdateXxx bodies plus OnVideoRestart on the DLL, so `RtSave_SaveLoad_Update`
// — which is ours — dispatched into retail five ways. This file closes that.
// Every body below was decompiled AND disassembled at its real address in the
// SHIPPED Runtime-x64.dll (image base 0x180000000) on 2026-09-09.
//
//   0x1800c0f10  ?OnVideoRestart@SaveLoad@Save@@QEAAXXZ
//   0x1800c1ea0  ?UpdateList@SaveLoad@Save@@QEAAXXZ
//   0x1800c2560  ?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ
//   0x1800c2690  ?UpdateLoad@SaveLoad@Save@@QEAAXXZ
//   0x1800c2a80  ?UpdateSave@SaveLoad@Save@@QEAAXXZ
//   0x1800c2d70  ?UpdateDelete@SaveLoad@Save@@QEAAXXZ
//
// ----------------------------------------------------------------------------
// THE HAZARD, restated from saveload.cpp because it is sharper here.
//
// A wrong port in this file does not crash. It desyncs a save, or deletes the
// wrong file, and surfaces later on an innocent record. Wave 5 chose its oracle
// shape BEFORE writing each body for exactly that reason, and used three:
// FULL DIFF, SCRATCH-COPY DIFF, and (declared as such) NONE.
//
// **None of those three is available to this file, and that is the whole
// problem it had to solve first.** These six bodies:
//
//   * are not read-only or idempotent, so FULL DIFF would delete a file twice,
//     begin two async loads, and signal two events;
//   * do not confine their writes to `*this`, so there is no scratch extent to
//     copy — UpdateList writes through `_savesData` into a heap array it also
//     reallocates, and OnVideoRestart writes into the Video::Device singleton;
//   * are guard-driven state machines, which is the shape that defeats replay
//     even when replay is otherwise possible: our body advances `_progress`,
//     so retail running second takes a DIFFERENT branch and the compare passes
//     while the branch under test never ran.
//
// So the oracle here is `runtime/funcs/rt_predict.h` — the PREDICTIVE shadow,
// where OUR body runs in a mode that writes nothing and merely records the
// value it WOULD have written at each location, THEIRS then runs for real on
// pristine state, and the two are compared location by location. The authority
// is the shipped DLL, not our own expectation. This is deliberately NOT
// `RT_DIFF_SHADOW`: the ~250 shadow sites in this tree take the port's own
// verdict and never consult the DLL, so a shared misunderstanding of retail
// passes every one of them
// (reference_shadow_marker_is_not_an_oracle.md).
//
// Each body is therefore written ONCE, against an `RtSink`. A live sink
// stores; a predicting sink records. One body, one set of offsets, one control
// flow — a separate predictor would be its own divergence hazard.
//
// GATE: BANISHED_RT_PREDICT=1, with BANISHED_RT_PREDICT_SELFTEST=1 as the
// negative control. Off by default and free when off.
//
// ----------------------------------------------------------------------------
// WHAT THE PREDICTOR CANNOT REACH, stated per body rather than in general,
// because a partial oracle that reads like a full one is worse than none:
// every suppressed callee's footprint is declared with `Exclude()`, and the
// per-site summary prints `excluded=` so the hole stays countable.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saveload_internal.h"
#include "../../funcs/rt_predict.h"

using RtSave::SaveLoad;
using RtSave::SavedState;
using RtSave::SaveInfo;
using RtSave::Operation;
using RtSystem::String;

using RtSaveDetail::MemAlloc;
using RtSaveDetail::MemFree;
using RtSaveDetail::PersistenceSingleton;
using RtSaveDetail::RtDataExport;
using RtSaveDetail::ZeroTime;

// runtime/src/video/buffers_win.cpp. These are the INLINE COPIES of the device
// lock — the ones that re-read `Device::_singleton` themselves and store the
// owner id BEFORE the depth — which is exactly the sequence OnVideoRestart
// carries at 0x1800c0f5a..0x1800c0f77. The exported Video::Device::Lock is a
// different function with the two stores in the other order; using it here
// would be a normalisation, not a transcription.
void RtVideoDeviceLock(void** deviceSingleton);
void RtVideoDeviceUnlock(void** deviceSingleton);

namespace {

// ---- dependencies, all of them REAL exports in the shipped table -----------
// Checked with pefile against C:\GOG Games\Banished\Runtime-x64.dll before
// each name was written down, never copied out of Ghidra's demangler
// (reference_resolveexport_phantom_names.md).
using BeginDeleteFn = bool (*)(void* persistence, const char** name);
using RecreateFn    = void (*)(void* imageBuffer);

// The vtable slot OnVideoRestart calls: `MOV R10,[RCX]` then
// `CALL qword ptr [R10+0x28]`, i.e. index 5 of the object hanging off
// ImageBuffer+0x60. Three real arguments follow `this`; the fourth operand in
// Ghidra's C output is the SEH state word (`MOV qword ptr [RSP+0x20],-0x2` in
// the prologue), not an argument.
using ImageUploadFn = void (*)(void* self, void* pixels, uint32_t w, uint32_t h);

void** DeviceSingletonSlot() {
    static void** p = RtDataExport<void*>("?_singleton@Device@Video@@0PEAV12@EA");
    return p;
}

// ---------------------------------------------------------------------------
// The System::String refcount idiom, SINK-AWARE.
//
// `RtSaveDetail::ReleaseStringSlot` is the live-only transcription and stays
// the definition of record; this is the same instruction sequence routed
// through the sink so a predicting round can see the refcount word it would
// have decremented and can SUPPRESS the free. Predicting a free is not
// optional politeness — in predict mode retail runs afterwards and frees the
// block itself, so a body that also freed it would double-free every round.
// ---------------------------------------------------------------------------
void ReleaseStringSlotSink(const char** slot, RtSink& out, const char* label) {
    const char* p = static_cast<const char*>(out.ReadPtr(slot));
    if (p == nullptr) return;
    const uint32_t rc = out.ReadU32(p - 4) - 1;
    out.U32(const_cast<char*>(p) - 4, rc, label);
    if (rc == 0) {
        out.Ptr(slot, p - 8, label);
        if (out.Call("System::MemoryAllocator::MemFree", (const void*)&MemFree,
                     reinterpret_cast<int64_t>(p - 8))) {
            MemFree(const_cast<char*>(p - 8));
        }
    } else {
        // Retail stores 0 into the member on the non-zero branch. A dead store
        // at both of this file's call sites, because both overwrite the member
        // immediately after — transcribed anyway, because "the compiler would
        // have elided it" is not a licence to elide it for retail.
        out.Ptr(slot, nullptr, label);
    }
}

void RetainIntoSink(const char** slot, const char* src, RtSink& out, const char* label) {
    out.Ptr(slot, src, label);
    if (src != nullptr) {
        out.U32(const_cast<char*>(src) - 4, out.ReadU32(src - 4) + 1, label);
    }
}

SaveInfo* SaveInfoAt(const SaveLoad* self, RtSink& out, int32_t index) {
    uint8_t* base = static_cast<uint8_t*>(out.ReadPtr(&self->_savesData));
    // MOVSXD RSI,dword ptr [RCX+0x48]; IMUL RSI,RSI,0x58 — a SIGN-extended
    // index, which matters because -1/-2/-4 are live sentinel values in this
    // class and a zero-extension would index 4 billion elements in.
    return reinterpret_cast<SaveInfo*>(base + static_cast<int64_t>(index) * 0x58);
}

// ============================================================================
// Save::SaveLoad::UpdateDelete()        retail 0x1800c2d70   ordinal 4514
// ----------------------------------------------------------------------------
// The simplest of the five, and the only one that calls no unexported internal
// at all: two states, one exported callee.
//
//   state 0  point `_string20` at the selected save's name (the refcount idiom,
//            skipped entirely when the two pointers are already equal), ask
//            IO::Persistence to begin the delete, advance to state 1.
//   state 1  unconditionally finish: clear the operation, reset the timer to
//            System::Time(0.0f), clear the progress, and put `_selectedIndex`
//            back to -1.
//
// NOTE WHAT STATE 1 DOES NOT DO. Its three siblings (UpdateLoad, UpdateSave,
// UpdateList) gate their identical tail on `_minOperationTime < _elapsed`, the
// minimum-spinner-time rule. UpdateDelete does NOT — 0x1800c2e0b is a bare
// `CMP EAX,0x1` with no time compare anywhere in the body. A delete therefore
// completes on the very next tick regardless of SetMinOperationTime. That
// asymmetry looks exactly like the kind of thing a porter "cleans up"; it is
// retail, it is deliberate on retail's part or not, and it is transcribed.
//
// ORACLE: PREDICTIVE, and this body is the one that makes the round mean
// something — its state-1 arm writes four fields whose predicted values differ
// from their pre-values on every real delete, so `changed=` is non-zero rather
// than the vacuous green a body of no-op predictions would produce.
// ============================================================================
void UpdateDeleteBody(SaveLoad* self, RtSink& out) {
    const uint32_t progress = out.ReadU32(&self->_progress);

    if (progress == 0) {
        const char* cur  = static_cast<const char*>(out.ReadPtr(&self->_string20));
        const SaveInfo* si = SaveInfoAt(self, out, self->_selectedIndex);
        const char* want = si->name;

        // `CMP RAX,qword ptr [RSI+RBP*0x1+0x38]` / `JZ` — when the slot already
        // holds this exact pointer, retail skips BOTH halves of the idiom, so
        // the refcount is not bumped and not dropped.
        if (cur != want) {
            ReleaseStringSlotSink(&self->_string20, out, "_string20 release");
            RetainIntoSink(&self->_string20, want, out, "_string20 = saves[sel].name");
        }

        void* persistence = PersistenceSingleton();
        BeginDeleteFn beginDelete =
            RT_EXPORT_ONCE(BeginDeleteFn, "?BeginDelete@Persistence@IO@@QEAA_NAEBVString@System@@@Z");
        if (out.Call("IO::Persistence::BeginDelete", (const void*)beginDelete,
                     reinterpret_cast<int64_t>(persistence),
                     reinterpret_cast<int64_t>(&self->_string20))) {
            beginDelete(persistence, &self->_string20);
        }

        out.U32(&self->_progress, 1, "_progress = 1");
        return;
    }

    if (progress == 1) {
        // Transcribed in retail's store order (0x1800c2e15 .. 0x1800c2e2e):
        // operation, progress, selectedIndex, then the converted time last.
        out.U32(&self->_operation,     0,          "_operation = kOpNone");
        out.U32(&self->_progress,      0,          "_progress = 0");
        out.U32(&self->_selectedIndex, 0xFFFFFFFFu, "_selectedIndex = -1");
        out.U64(&self->_elapsed, static_cast<uint64_t>(ZeroTime()), "_elapsed = Time(0.0f)");
    }
}

// ============================================================================
// Save::SaveLoad::OnVideoRestart()      retail 0x1800c0f10   ordinal 3296
// ----------------------------------------------------------------------------
// Walks every SaveInfo and re-uploads its thumbnail after a device reset:
// `ImageBuffer::Recreate`, then, under the Video::Device lock, one virtual
// upload call with the saved pixel block and its dimensions.
//
// The guard is TWO conditions and both matter: the ImageBuffer pointer at
// SaveInfo+0x18 must be non-null AND the int at SaveInfo+0x40 must be >= 0.
// That second one is the same "has metadata been loaded for this row yet"
// sentinel UpdateList uses at 0x1800c1f8c and 0x1800c2d2b; a row still waiting
// on its metadata has a live ImageBuffer and an unloaded thumbnail, and
// uploading it would push a garbage block at the GPU.
//
// FIELDS THIS BODY PINS. SaveInfo+0x18/+0x20/+0x24/+0x28/+0x40 were all inside
// the `unknown10`/`unknown40` holes that Wave 5's header declared explicitly
// unmodelled ("written by UpdateList/LoadMetaData, neither of which this lane
// ports"). This wave ports both, so the header now names them.
//
// ORACLE: PREDICTIVE, AND DELIBERATELY THIN — said here rather than left for a
// reader to discover from a green board. This body performs almost no writes
// of its own: its entire effect is two calls, and everything either of them
// changes lives inside the ImageBuffer and the GPU resource. So the round
// predicts only the device lock's owner/depth pair, and `Call()` records the
// suppressed targets and arguments. What that DOES verify is worth stating
// precisely, because it is not nothing and it is not everything:
//
//   VERIFIED  which rows are selected (the two-part guard), the iteration
//             order and stride, the three argument values per row, and the
//             identity of the vtable slot — all of these are recorded per call
//             and compared against the targets retail loads.
//   NOT       what Recreate and the upload actually do. Both footprints are
//             declared with Exclude(), and the site prints `excluded=`.
//
// A round on this site will report `changed=0` whenever the lock is balanced,
// which the oracle correctly prints as UNPROVEN rather than as passing. That
// is the honest reading: the argument trace is the evidence here, not the
// post-state.
// ============================================================================
void OnVideoRestartBody(SaveLoad* self, RtSink& out, RtPredictRound* round) {
    const int32_t count = self->_savesSize;
    for (int32_t i = 0; i < count; ++i) {
        uint8_t* row = static_cast<uint8_t*>(out.ReadPtr(&self->_savesData))
                     + static_cast<int64_t>(i) * 0x58;

        void* buffer = *reinterpret_cast<void**>(row + 0x18);
        if (buffer == nullptr) continue;
        if (*reinterpret_cast<const int32_t*>(row + 0x40) < 0) continue;

        RecreateFn recreate =
            RT_EXPORT_ONCE(RecreateFn, "?Recreate@ImageBuffer@Video@@QEAAXXZ");
        if (out.Call("Video::ImageBuffer::Recreate", (const void*)recreate,
                     reinterpret_cast<int64_t>(buffer))) {
            recreate(buffer);
        } else if (round != nullptr) {
            // Recreate reallocates the GPU-side resource and rewrites the
            // ImageBuffer's own fields. Not modelled; declared so the coverage
            // control counts it instead of reporting it as an unpredicted write.
            round->Exclude(buffer, 0x80, "Video::ImageBuffer::Recreate footprint");
        }

        void** device = DeviceSingletonSlot();
        const bool live = !out.predicting();
        if (live) RtVideoDeviceLock(device);

        // Retail RE-READS `_savesData` here (0x1800c0f7d) rather than reusing
        // the pointer it loaded before the lock. Transcribed: Recreate can run
        // arbitrary code, and the re-read is what retail relies on.
        uint8_t* row2 = static_cast<uint8_t*>(out.ReadPtr(&self->_savesData))
                      + static_cast<int64_t>(i) * 0x58;
        void*  target = *reinterpret_cast<void**>(row2 + 0x18);
        void*  obj    = *reinterpret_cast<void**>(static_cast<uint8_t*>(target) + 0x60);
        void** vtbl   = *reinterpret_cast<void***>(obj);
        ImageUploadFn upload = reinterpret_cast<ImageUploadFn>(vtbl[5]);

        void* pixels = *reinterpret_cast<void**>(row2 + 0x28);
        uint32_t w   = *reinterpret_cast<const uint32_t*>(row2 + 0x20);
        uint32_t h   = *reinterpret_cast<const uint32_t*>(row2 + 0x24);

        if (out.Call("ImageBufferData::Upload [vtbl+0x28]", (const void*)upload,
                     reinterpret_cast<int64_t>(obj),
                     reinterpret_cast<int64_t>(pixels), w, h)) {
            upload(obj, pixels, w, h);
        } else if (round != nullptr) {
            round->Exclude(obj, 0x40, "ImageBufferData upload footprint");
        }

        if (live) RtVideoDeviceUnlock(device);
    }
}

}  // namespace

// ============================================================================
// The registered entry points.
//
// Each is the same three lines: declare the site, and either run a predicting
// round (ours predicts, theirs runs for real, Verify compares) or run the body
// live. `RT_PREDICT_ROUND`'s increment expression is `Verify()`, so an early
// return out of the block still verifies rather than silently dropping a round
// (reference_uncalled_port_is_not_verified.md is about the neighbouring
// failure: a green line over a body nothing ever entered).
// ============================================================================

void RtSave_SaveLoad_UpdateDelete(SaveLoad* self) {
    RT_PREDICT_SITE("Save::SaveLoad::UpdateDelete");
    if (RT_PREDICT_ON) {
        RT_PREDICT_ROUND(round) {
            round.Watch(self, sizeof(SaveLoad), "Save::SaveLoad");
            UpdateDeleteBody(self, round.sink());
            using Fn = void (*)(SaveLoad*);
            RT_ORIGINAL_ONCE(Fn, "?UpdateDelete@SaveLoad@Save@@QEAAXXZ")(self);
        }
        return;
    }
    RtSink live;
    UpdateDeleteBody(self, live);
}

void RtSave_SaveLoad_OnVideoRestart(SaveLoad* self) {
    RT_PREDICT_SITE("Save::SaveLoad::OnVideoRestart");
    if (RT_PREDICT_ON) {
        RT_PREDICT_ROUND(round) {
            round.Watch(self, sizeof(SaveLoad), "Save::SaveLoad");
            OnVideoRestartBody(self, round.sink(), &round);
            using Fn = void (*)(SaveLoad*);
            RT_ORIGINAL_ONCE(Fn, "?OnVideoRestart@SaveLoad@Save@@QEAAXXZ")(self);
        }
        return;
    }
    RtSink live;
    OnVideoRestartBody(self, live, nullptr);
}

// ============================================================================
// Registration. Both doors, for the reason saveload.cpp's own registration
// block states: a name door alone is one typo away from resolving NULL in
// silence, and an address door alone is invisible to anything that resolves by
// name. `n` counts PORTED FUNCTIONS, so the address registrations do not
// increment it — they are second doors onto functions already counted, and
// incrementing would inflate the census figure this project treats as
// authoritative over any agent's claim.
// ============================================================================
int RtSaveLoadUpdateRegister() {
    int n = 0;
    RegisterRuntimeExport("?UpdateDelete@SaveLoad@Save@@QEAAXXZ",
                          (void*)&RtSave_SaveLoad_UpdateDelete); ++n;
    RegisterRuntimeAddr(0x1800c2d70, (void*)&RtSave_SaveLoad_UpdateDelete);
    RegisterRuntimeExport("?OnVideoRestart@SaveLoad@Save@@QEAAXXZ",
                          (void*)&RtSave_SaveLoad_OnVideoRestart); ++n;
    RegisterRuntimeAddr(0x1800c0f10, (void*)&RtSave_SaveLoad_OnVideoRestart);
    return n;
}
