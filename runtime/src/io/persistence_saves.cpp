// ============================================================================
// runtime/src/io/persistence_saves.cpp — Wave 19 lane PERSIST
// ============================================================================
//
// The save-enumeration and request-issue half of IO::Persistence: six of the
// lane's seven rows. The seventh, ?BeginGetSaveGameInfo@, is the only one that
// touches the Win32 event API directly and lives in the _win twin next door
// (PLAN.md §4.1: runtime/src/ stays platform-neutral unless a file is
// explicitly a _win file).
//
//   ?GetSaveCount@Persistence@IO@@QEBAHXZ                    0x1800b7c60
//   ?GetSaveInfo@Persistence@IO@@QEBA?AUSaveGameInfo@12@H@Z  0x1800b7bd0
//   ?GetSaveFilename@Persistence@IO@@AEBA?AVFilename@...     0x1800b7c80
//   ?BeginDelete@Persistence@IO@@QEAA_NAEBVString@System@@@Z 0x1800b7ab0
//   ?BeginLoad@Persistence@IO@@QEAA_N...PEBUSaveBlock@12@@Z  0x1800b7970
//   ??1Persistence@IO@@QEAA@XZ                               0x1800b7fb0
//
// Every address above was checked against the shipped export table before a
// line was written (pefile over Runtime-x64.dll: ordinals 2884, 2888, 2887,
// 1638, 1647, 647), so none is a phantom
// (reference_resolveexport_phantom_names.md). Layout is pinned in
// runtime/types/io/Persistence.h, which this lane extended with the worker
// fields at +0x10..+0x8f; the offsets there were each read twice, once
// Persistence-relative and once worker-relative, and the two agree.
//
// THE ASYNC MECHANISM — one finding, not three
// --------------------------------------------
// The lane was briefed to expect a stored completion callback, because that
// single root cause held back all six Wave 8 Audio rows
// (reference_audio_holdbacks_share_a_callback_root_cause.md). THERE IS NO
// STORED CALLBACK ANYWHERE IN THIS CLUSTER. Reading all three Begin* bodies
// and both private enqueue helpers (0x1800b7390, 0x1800b7460) as
// disassembly: not one CALL RAX, not one indirect call through an object
// field. The asynchrony is a System::Thread embedded at Persistence+0x10,
// driven by
//
//     command word at +0x4c   (1 load - 2 delete - 3 info - 4 list)
//     status  word at +0x48   (?GetStatus@, 0x1800b7c40, reads it)
//     ResetEvent(doneEvent) ; SetEvent(workEvent)
//
// and every one of the three Begin* is the SAME three-part shape:
//
//     if (this->flags & 1) return false;             // refuse
//     GetSaveFilename(this, &tmp, name);             // "<savePath>Save\<name>.sav"
//     <issue: fill the command fields, then Reset/SetEvent>
//     release tmp; return true;
//
// So the three rows share one mechanism, and that mechanism is strictly
// friendlier than Audio's: the whole issued state is a String, three ints and
// two event handles, all inside the object, with nothing having yet run into
// game state when the call returns. That is what makes the synthesised
// whole-object oracle in persistence_saves_probe.cpp possible at all.
//
// WHAT IS CALLED BY ADDRESS, AND WHY
// ----------------------------------
// The two enqueue helpers and the worker destructor are UNEXPORTED DLL
// internals — checked, not assumed: absent from the export table by address.
// That is the one case where "AEAA means PRIVATE, not unexported" does NOT
// apply (reference_aeaa_is_private_not_unexported.md); these have no mangled
// name at all. They are called by address exactly as persistence.cpp already
// calls Map_Find/Map_Insert, and for the same reason: they are shared DLL
// internals this lane does not own. THIS GROWS THE CUTOVER SURFACE — see the
// set-diff in docs/runtime_port/WAVE19_LANE_PERSIST.md
// (reference_porting_can_grow_the_cutover_surface.md).
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>        // placement new into the retail hidden return slot
#include <utility>    // std::move -- the pointer steal out of a POD out-param
#include "../../types/io/Persistence.h"
#include "../../types/io/PersistenceWorker.h"   // ENDGAME B.3.9 IO (W2)
#include "../system/string.h"
#include "../system/pod_bridge.h"
#include "../../types/collection/Collection.h"   // rtsetclear::Clear

using RtSystem::String;
using RtSystem::Sys;   // the transitional POD->class bridge (pod_bridge.h)
using RtIo::Persistence;
using RtIo::SaveGameInfo;
using RtIo::SaveBlock;

// B3R RT-IO: the three unexported DLL internals this file reached by address
// are called by name: RtIo_PersistenceWorker_EnqueueDelete (0x1800b7390,
// command 2), RtIo_PersistenceWorker_EnqueueLoad (0x1800b7460, command 1) --
// both persistence_worker_win.cpp -- and rtsetclear::Clear (0x18000f7d0, the
// settings-map dtor; ENDGAME B.3.2's reason for not using `Collection::Set<T>`
// still holds: `SettingsMapNode`'s two fields are the old POD String).


// Retail's own format string, read out of .rdata at 0x180155840.
const char kRtPersistSaveFilenameFormat[] = "%sSave\\%s.sav";

// ============================================================================
// int IO::Persistence::GetSaveCount() const                  retail 0x1800b7c60
// ----------------------------------------------------------------------------
// The whole body is four instructions:
//   testb $1,0x18(rcx) ; je L ; xor eax,eax ; ret ; L: mov 0x88(rcx),eax ; ret
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 6: the bodies in this file are members of the
// real `IO::Persistence`, at the bottom of this file.

// ============================================================================
// SaveGameInfo IO::Persistence::GetSaveInfo(int index) const retail 0x1800b7bd0
// ----------------------------------------------------------------------------
// RETURNED BY VALUE, so the real ABI is (this=RCX, hiddenSlot=RDX, index=R8D)
// and RAX comes back holding the slot. Omitting that parameter from the
// typedef would be a self-consistent wild write
// (reference_hidden_retslot_wild_write.md) — Persistence.h's SaveGameInfo
// banner says the same thing, and 0x1800b7bd0's own first instruction
// (`mov %rdx,0x10(%rsp)` — spilling the slot, not an int) is the proof.
//
// FAITHFUL: there is NO bounds check on `index`. Retail indexes the array
// unconditionally on the non-refused branch. Do not add one — this project's
// rule is that an unguarded deref is load-bearing (CLAUDE.md).
// ============================================================================

// ============================================================================
// System::Filename IO::Persistence::GetSaveFilename(const String&) const
// retail 0x1800b7c80 — PRIVATE (`AEBA`), and exported all the same: this DLL
// exports its private members, so it is registered BY NAME like any other row
// (reference_aeaa_means_private_not_unexported.md). Also returns BY VALUE:
// (this=RCX, hiddenSlot=RDX, const String& name=R8).
// ============================================================================

// ============================================================================
// bool IO::Persistence::BeginDelete(const String& name)      retail 0x1800b7ab0
// ============================================================================

// ============================================================================
// bool IO::Persistence::BeginLoad(const String& name, int blockCount,
//                                 const SaveBlock* blocks)   retail 0x1800b7970
// ============================================================================

// ============================================================================
// IO::Persistence::~Persistence()                            retail 0x1800b7fb0
// ----------------------------------------------------------------------------
// Three steps and nothing else: release _savePath, destroy the embedded
// worker, then TAIL-JUMP into the settings-map destructor (0x18000f7d0, the
// same body Wave 9 lane BOOT2 read to pin +0x00/+0x08). Written as a plain
// call here — the tail jump is a code-size optimisation with no observable
// effect, and it is the only deviation from the shipped instruction sequence
// in this file.
//
// NOT VERIFIED BY ORACLE, deliberately. See the wave doc's "what this does NOT
// claim": ours-then-theirs would destroy one object twice, and the
// synthesised-scratch route used for the Begin* is not available because
// 0x1800b7740 joins and closes a REAL thread — handing it a fabricated worker
// is not a controlled experiment, it is a crash with extra steps.
// ============================================================================

// ============================================================================
// `IO::Persistence`'s member bodies for this file
// ============================================================================
// Defined in `namespace IO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2). The file-local
// helpers live in this translation unit's GLOBAL anonymous namespace, so they
// are spelled with the `::` on purpose, so a future reader cannot
// mistake them for members. Every POD is spelled `RtIo::` or `RtSystem::` and
// every real class `System::` (B34_RULES section 7.2); note this file's POD
// namespace is `RtIo`, LOWERCASE o.
//
// NOTHING ELSE CHANGED: same callees in the same order, the same refusal test
// on bit 0 of `_flags` first, the same absence of a bounds check in
// GetSaveInfo, the same three-step destructor. The three `GetSaveFilename`
// calls inside BeginDelete / BeginLoad / (in the _win twin)
// BeginGetSaveGameInfo become direct MEMBER calls of the same body: they were
// already direct calls to `RtIo_Persistence_GetSaveFilename` in this
// translation unit and never went through a resolve door, so which body runs is
// unchanged -- what goes away is the POD round trip the free-function form
// needed, which was one pointer steal and no AddRef either way.
namespace IO {

int Persistence::GetSaveCount() const {
    int ours = (_flags & 1u) ? 0 : _saveCount;

    return ours;
}

RtIo::SaveGameInfo* Persistence::GetSaveInfo(RtIo::SaveGameInfo* ret, int index) const {

    // `ret` is the hidden return slot -- raw, uninitialised bytes -- so
    // `ret->name` (a `RtSystem::String` field of `SaveGameInfo`, which stays a
    // POD) is placement-constructed through the transitional bridge.
    // `String(const String&)` (retail 0x180006cb0) is exactly "zero, then
    // AddRef if non-null", which is what the C body wrote out by hand.
    //
    // FAITHFUL: there is still NO bounds check on `index`.
    if (_flags & 1u) {
        ret->sortKey = 0;
        new (&::Sys(ret->name)) System::String();
    } else {
        const RtIo::SaveGameInfo& e = _saves[index];
        ret->sortKey = e.sortKey;
        new (&::Sys(ret->name)) System::String(::Sys(e.name));
    }

    return ret;
}

System::Filename Persistence::GetSaveFilename(const RtSystem::String& nameRef) const {
    const RtSystem::String* const name = &nameRef;

    // ?Format@String@System@@SA?AV12@PEBDZZ (retail 0x1800c9730), by name.
    System::String tmp = System::String::Format(
        ::kRtPersistSaveFilenameFormat, _savePath.data, name->data);

    // The manual "zero, AddRef-adopt tmp's buffer, then AdjustDelimiters" IS
    // `Filename(const String&)` (retail 0x18000c780) -- see B31_RULES
    // DELIMITERS: an Adopt onto a just-nulled destination followed by
    // AdjustDelimiters is exactly that ctor. The DOOR's placement-new is
    // retail's hidden-return-slot construction.
    System::Filename out(tmp);

    // ~tmp is retail's release.
    return out;
}

bool Persistence::BeginDelete(const RtSystem::String& nameRef) {
    const RtSystem::String* const name = &nameRef;

    if (_flags & 1u) return false;

    // GetSaveFilename is retail's own member and is now ours, so this is the
    // direct member call retail's `CALL 0x1800b7c80` is. C++17 constructs the
    // result STRAIGHT INTO `fn` -- no copy, no extra AddRef, exactly the one
    // object the POD-plus-move round trip produced. `AsString()` is the same
    // eight bytes (Filename IS a String, layout-identically), which is what
    // retail hands the enqueue helper.
    System::Filename fn(GetSaveFilename(*name));
    RtIo_PersistenceWorker_EnqueueDelete(&_workerVtable,             // 0x1800b7390
        reinterpret_cast<const RtSystem::String*>(&fn.AsString()));      // B.3.x bridge
    // ~fn is retail's release.
    return true;
}

bool Persistence::BeginLoad(const RtSystem::String& nameRef, int blockCount,
                            const RtIo::SaveBlock* blocks) {
    const RtSystem::String* const name = &nameRef;

    if (_flags & 1u) return false;

    // Same shape as BeginDelete above.
    System::Filename fn(GetSaveFilename(*name));
    RtIo_PersistenceWorker_EnqueueLoad(&_workerVtable,               // 0x1800b7460
        reinterpret_cast<const RtSystem::String*>(&fn.AsString()),       // B.3.x bridge
        blockCount, blocks);
    // ~fn is retail's release.
    return true;
}

Persistence::~Persistence() {

    // `_savePath` is a `RtSystem::String` member and stays one (the header says
    // why): retail's release is spelled as the String destructor through the
    // bridge, BY HAND, exactly here. Retyping it would add an implicit member
    // destructor running in ADDITION to this line.
    ::Sys(_savePath).~String();
    // ENDGAME B.3.9 IO (W2): the embedded worker's destructor (0x1800b7740),
    // called directly where retail calls it -- the class's own, not a door.
    reinterpret_cast<PersistenceWorker*>(&_workerVtable)->~PersistenceWorker();
    rtsetclear::Clear(reinterpret_cast<rtsetavl::Set*>(this));        // 0x18000f7d0  B.3.x bridge
}

}  // namespace IO

// ---------------------------------------------------------------------------
