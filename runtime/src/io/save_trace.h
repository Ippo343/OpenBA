#pragma once
// ============================================================================
// runtime/src/io/save_trace.h — Wave 9 lane S: the save-stream A/B oracle
// ============================================================================
//
// WHY THIS EXISTS.
//
// Four ports are held back with the same stated reason, across three waves:
// `Audio::Scene::Serialize`, `ExternalFactory::SerializeReferenceTable`,
// `External::SaveText`, and Wave 3's `InternalBasePtr::Serialize` (WAVE3.md:
// "has no oracle at all and lands in the save/load path"). The reasoning was
// that a Serialize cannot be replayed -- call ours then theirs and the stream
// is consumed or written twice.
//
// THAT IS ONLY TRUE OF THE READ PATH, and this file is the consequence.
//
// **On the WRITE path a Serialize is a pure read of the object.** It walks the
// object and emits; it does not mutate what it is serializing. So calling ours
// and then theirs IS a legitimate replay -- provided the two calls emit into
// DIFFERENT SINKS. What made replay impossible was never the function, it was
// that both calls shared one stream.
//
// WHAT IS COMPARED, and why it is not the bytes on disk. A desync IS "a
// different sequence of typed writes", so the signal is the EMISSION SEQUENCE,
// not the buffer. That makes the sink cheap: no real `MemoryStream` and no
// codec, just a stub `IO::Stream` whose `direction` says writing and whose
// `encoding` points at a stub `IO::Encoding` whose reader/writer factory hands
// back a stub `IO::ReaderWriter` recording `(slot, args, value)` per call.
//
// Every offset the doubles need was already measured by Wave 4 and lives in
// `runtime/types/io/Serializer.h`: `Serializer+0x00` is the `Stream*`,
// `Stream+0x08` is the direction (0 == reading), `Stream+0x10` is the
// `Encoding*`, and `rw_vtbl` maps all 26 `ReaderWriter` slots at RETAIL
// offsets -- the ones this project has a standing warning about because the
// profile build's differ (bool +0x78/+0x90, uint +0x98/+0xb0, int +0xa0/+0xb8).
//
// SAFETY. `Compare` runs BOTH bodies against synthetic sinks, never against the
// real save stream, and only ever in the writing direction -- so neither call
// can touch the game's save, and neither mutates the object under test. It is
// A3-HOLD: its "theirs" arm was the shipped DLL, so Enabled() is now always
// false and the instrument is dead. Callers outside this directory still
// reference it; it dies with them.
//
// WHAT IT DOES NOT COVER. The read path, which consumes the stream and mutates
// the object, stays hard. It is also the half a write-path oracle indirectly
// protects: if our writer emits the sequence retail's writer emits, a retail
// reader accepts our save, which is the property that actually matters.
// ============================================================================

#include <cstdint>

namespace RtSaveTrace {

// Always false: the second arm was the shipped DLL.
bool Enabled();

// Run `ours` and `theirs` against two independent recording sinks and compare
// the emission sequences. `obj` is the `this` for both. Both are called as
//     void f(void* self, Serializer* ser)
// which is the shape of every member in the held-back family.
//
// Returns true when the two sequences are identical. Reports through the same
// [RtSaveTrace] channel either way, naming the FIRST differing emission --
// slot, index and value -- because "they differ" is not actionable and
// "emission 41 wrote int 7 where retail wrote int 0" is.
// `stopAfter` > 0 runs each side only until it has made that many emissions,
// then abandons the rest of the body. THIS IS THE PREFIX ORACLE, and it exists
// because the pure-read property above does NOT hold for every member of the
// family:
//
//   Audio::Scene::Serialize            mutates nothing on the write path (its
//                                      resize is guarded to the reading side)
//   ExternalFactory::SerializeReferenceTable  writes an id into every node
//   InternalBasePtr::Serialize         ends in PersistObject -- mutates the
//                                      pointer AND the global factory
//
// For the mutating ones, replaying the WHOLE body performs the mutation twice
// on the one live tree or factory: the same shared-state trap the family was
// held back for. Stopping before the first mutation keeps the half that decides
// the stream layout -- the emission sequence -- and skips the half that cannot
// be run twice.
//
// THE COST, stated because a partial oracle that reads like a full one is worse
// than none: everything after the stop is NOT compared, and the report says so
// on every line. Choose `stopAfter` from the disassembly -- the emission index
// of the last call before the first store -- not by trial.
//
// Implemented with setjmp/longjmp, so the abandoned frames are retail's own and
// must have nothing that needs unwinding. That holds for this family (no C++
// destructors, no lock held across the emissions); it is not a general licence.
bool Compare(const char* name, void* obj,
             void (*ours)(void*, void*), void (*theirs)(void*, void*),
             int stopAfter = 0);

// Totals for the end-of-run summary.
void Summary();

}  // namespace RtSaveTrace
