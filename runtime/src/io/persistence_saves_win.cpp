// ============================================================================
// runtime/src/io/persistence_saves_win.cpp — Wave 19 lane PERSIST
// ============================================================================
//
// One row: ?BeginGetSaveGameInfo@Persistence@IO@@QEAA_NAEBVString@System@@H@Z
// (retail 0x1800b7870, ordinal 1642).
//
// It sits in a _win file for one reason: it is the ONLY member of this lane's
// seven that reaches the Win32 event API in its own body. Its two siblings
// (BeginDelete, BeginLoad) reach the same two calls, but through an
// unexported DLL helper, so their C++ stays platform-neutral and lives in
// persistence_saves.cpp. That asymmetry is retail's, not ours: 0x1800b7870
// does not delegate to an enqueue helper, it inlines the whole issue
// sequence.
//
// The two imports are declared here rather than pulled in from <windows.h>,
// which rt_port.h's banner explains cannot be included anywhere under
// runtime/src/ (it #defines `near`/`far` away and declares a function called
// `Rectangle`). The IAT slots were identified from the shipped binary, not
// guessed: 0x18014b198 is KERNEL32!ResetEvent and 0x18014b368 is
// KERNEL32!SetEvent, read out of Runtime-x64.dll's import directory.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>    // std::move -- the pointer steal out of a POD out-param
#include "../../types/io/Persistence.h"
#include "../../types/collection/Collection.h"   // ENDGAME B.3.2 step 2
#include "../system/string.h"
#include "../system/pod_bridge.h"

using RtSystem::String;
using RtSystem::Sys;   // the transitional POD->class bridge (pod_bridge.h)
using RtIo::Persistence;

// GetSaveFilename (persistence_saves.cpp) is called directly as the member:
// retail's 0x1800b789f is a direct rel32 CALL to 0x1800b7c80.

extern "C" __declspec(dllimport) int __stdcall ResetEvent(void* handle);
extern "C" __declspec(dllimport) int __stdcall SetEvent(void* handle);

namespace {

// ENDGAME B.3.2 step 2 (2026-09-13) — both are `Collection::Array<T>::~Array()`
// (array.cpp's J1 family, RegisterRuntimeAddr'd as RtColl_ArrayDtor_*), NOT a
// Clear (the old local name was misleading: they free the buffer, they do not
// keep it). Both element types are trivially destructible, so the
// trait-derived instantiation IS the registered body with no hand-derived
// spec: `BlockDesc12` mirrors persistence_saveinfo_job.cpp's `BlockDesc`
// (three dwords, stride 0xc, matching the block-descriptor array at
// Persistence+0x60) and `void*` is `blockPtrData` at +0x70. Both fields are
// still loose (data ptr, count, pad) rather than typed `Collection::Array<T>`
// members of `Persistence` (shared with files outside this batch), so these
// are reinterpret_cast bridges at the use site.
struct BlockDesc12 { uint32_t id, offset, size; };
static_assert(sizeof(BlockDesc12) == 0xC, "matches kSpec_Blk12's stride");

void ClearBlockDescs(::IO::Persistence* p) {
    reinterpret_cast<Collection::Array<BlockDesc12>*>(&p->_blockDescData)->~Array();  // 0x18002a930  // LEFT: H5 Persistence::_blockDescData (raw Array storage; behaviour commit owed)
}
void ClearBlockPtrs(::IO::Persistence* p) {
    reinterpret_cast<Collection::Array<void*>*>(&p->_blockPtrData)->~Array();         // 0x180024aa0  // LEFT: H5 Persistence::_blockPtrData (as above)
}

}  // namespace

// ============================================================================
// bool IO::Persistence::BeginGetSaveGameInfo(const String& name, int count)
// retail 0x1800b7870
// ----------------------------------------------------------------------------
// Instruction-order note: retail writes `command = 3` (0x1800b78b7) BEFORE the
// filename assignment and `status = 0` (0x1800b7903) after it. The order is
// preserved below even though nothing in this thread can observe the
// difference — the worker thread can, and it is woken only by the SetEvent at
// the end, so any reordering here is a change to what a racing reader could
// see. Faithful transcription costs nothing; reasoning about the race costs
// a wave.
// ============================================================================
// The body is the member `IO::Persistence::BeginGetSaveGameInfo`, below.

// ============================================================================
// `IO::Persistence::BeginGetSaveGameInfo` -- the member body
// ============================================================================
// Defined in `namespace IO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2); the two file-local
// array-teardown helpers live in this translation unit's GLOBAL anonymous
// namespace and are reached as `::ClearBlockDescs` / `::ClearBlockPtrs`. Every
// POD is spelled `RtIo::` or `RtSystem::` and every real class `System::`
// (B34_RULES section 7.2); note this file's POD namespace is `RtIo`, LOWERCASE
// o.
//
// NOTHING ELSE CHANGED: same callees in the same order, the refusal test on bit
// 0 of `_flags` still first, the two array teardowns still between the filename
// and the command fields, the guarded String assignment still guarded on the
// BUFFER, and ResetEvent before SetEvent. The `GetSaveFilename` call, which was
// already a DIRECT call into the sibling file's door (retail's 0x1800b789f is a
// direct rel32 CALL), is now a direct MEMBER call of that same body -- so which
// body runs is unchanged, and the POD-plus-move round trip it needed collapses
// into one construction with the same single AddRef.
namespace IO {

bool Persistence::BeginGetSaveGameInfo(const System::String& name, int count) {

    if (_flags & 1u) return false;

    // C++17 constructs GetSaveFilename's result STRAIGHT INTO `fn`.
    // `AsString()` is the same eight bytes (Filename IS a String,
    // layout-identically), which is what retail assigns from.
    System::Filename fnFile(GetSaveFilename(name));
    const System::String& fn = fnFile.AsString();

    ::ClearBlockDescs(this);
    ::ClearBlockPtrs(this);

    _command = 3;

    // The guarded String assignment at 0x1800b78be..0x1800b7903 IS
    // `String::operator=` (retail 0x1800c9810): retail skips the whole
    // release/adopt when the two buffers are already the same pointer, which
    // is not just an optimisation — releasing first would drop the last
    // reference to the buffer it is about to re-adopt. `pendingFilename` is a
    // `RtSystem::String` member of the not-yet-converted `Persistence`,
    // reached through the one transitional bridge.
    if (_pendingFilename.data != fn._data) {
        ::Sys(_pendingFilename) = fn;
    }

    _status     = 0;
    _blockCount = count;

    ResetEvent(_doneEvent);
    SetEvent(_workEvent);

    // ~fnFile is retail's release.
    return true;
}

}  // namespace IO

// ---------------------------------------------------------------------------
