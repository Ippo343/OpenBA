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
#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"

using RtSystem::String;
using RtSystem::Sys;   // the transitional POD->class bridge (pod_bridge.h)
using RtIo::Persistence;
using RtIo::SaveGameInfo;
using RtIo::SaveBlock;

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---- unexported DLL internals, by address ----------------------------------
// File-local typedefs for calls OUT through the resolve door (not entry-point
// signatures), so they now name the class occupying the same eight bytes.
using EnqueueDeleteFn = void (*)(void* worker, const System::String* filename);
using EnqueueLoadFn   = void (*)(void* worker, const System::String* filename,
                                 int blockCount, const SaveBlock* blocks);
using WorkerDtorFn    = void (*)(void* worker);
using MapClearFn      = void (*)(void* self);

EnqueueDeleteFn EnqueueDelete() {   // 0x1800b7390 — command 2
    static auto fn = reinterpret_cast<EnqueueDeleteFn>(
        ResolveAddr(Module::Runtime, 0x1800b7390));
    return fn;
}
EnqueueLoadFn EnqueueLoad() {       // 0x1800b7460 — command 1
    static auto fn = reinterpret_cast<EnqueueLoadFn>(
        ResolveAddr(Module::Runtime, 0x1800b7460));
    return fn;
}
WorkerDtorFn WorkerDtor() {         // 0x1800b7740 — the embedded worker's dtor
    static auto fn = reinterpret_cast<WorkerDtorFn>(
        ResolveAddr(Module::Runtime, 0x1800b7740));
    return fn;
}
// ENDGAME B.3.2 step 2 (2026-09-13) — NOT CONVERTED to `Collection::Set<T>`,
// deliberately, for the same reason persistence_registry_win.cpp's MapClear
// is not (see that file's note): 0x18000f7d0 IS `Set<SettingsMapNode>::Clear()`
// and `Persistence` IS that Set's {root,count,pad} header, but `Clear()`'s
// `n->element.~T()` needs `SettingsMapNode`'s two fields to be the real
// `System::String` class to release their refcounts; they are still the old
// `RtSystem::String` POD, shared with files outside this batch
// (persistence_startup.cpp / closure_batch2b_win.cpp / engine/singleton_ctors.cpp).
MapClearFn MapClear() {             // 0x18000f7d0 — the settings-map dtor
    static auto fn = reinterpret_cast<MapClearFn>(
        ResolveAddr(Module::Runtime, 0x18000f7d0));
    return fn;
}

}  // namespace

// ---- exported dependencies, by NAME ----------------------------------------
// Both are already ported (String::Format by Wave 2 lane String,
// Filename::AdjustDelimiters per WAVE19_INTEGRATOR_CORRECTIONS.md C2), so the
// port-registry-aware door is the right one: the raw DLL door here would walk
// past our own ports into the DLL (rt_port.h's ??DQuaternion note).
using RtPersistFormatFn           = void* (*)(void* ret, const char* fmt, ...);

// Retail's own format string, read out of .rdata at 0x180155840.
const char kRtPersistSaveFilenameFormat[] = "%sSave\\%s.sav";

// ============================================================================
// int IO::Persistence::GetSaveCount() const                  retail 0x1800b7c60
// ----------------------------------------------------------------------------
// The whole body is four instructions:
//   testb $1,0x18(rcx) ; je L ; xor eax,eax ; ret ; L: mov 0x88(rcx),eax ; ret
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 6: the six bodies in this file move onto the
// real `IO::Persistence`, at the bottom of this file. Each door keeps its name,
// its signature and both registration lines, and delegates. No qualified `IO::`
// call is needed: `IO::Persistence` has no vtable of its own -- the vptr at
// +0x10 is the EMBEDDED worker's (runtime/types/io/Persistence.h) -- so
// B34_BRIEFS section A.1's re-entry hazard cannot arise.
int RtIo_Persistence_GetSaveCount(const void* self) {
    return reinterpret_cast<const IO::Persistence*>(self)->GetSaveCount();
}

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
void* RtIo_Persistence_GetSaveInfo(const void* self, void* retval, int index) {
    return reinterpret_cast<const IO::Persistence*>(self)->GetSaveInfo(
        static_cast<SaveGameInfo*>(retval), index);
}

// ============================================================================
// System::Filename IO::Persistence::GetSaveFilename(const String&) const
// retail 0x1800b7c80 — PRIVATE (`AEBA`), and exported all the same: this DLL
// exports its private members, so it is registered BY NAME like any other row
// (reference_aeaa_means_private_not_unexported.md). Also returns BY VALUE:
// (this=RCX, hiddenSlot=RDX, const String& name=R8).
// ============================================================================
// The return is BY VALUE, so `retval` IS retail's hidden return slot; the
// member's by-value result is CONSTRUCTED into it -- still exactly the one
// `Filename(const String&)` (0x18000c780) the body always ran.
void* RtIo_Persistence_GetSaveFilename(const void* self, void* retval, const void* nameRef) {
    new (static_cast<System::Filename*>(retval)) System::Filename(
        reinterpret_cast<const IO::Persistence*>(self)->GetSaveFilename(
            *static_cast<const String*>(nameRef)));
    return retval;
}

// ============================================================================
// bool IO::Persistence::BeginDelete(const String& name)      retail 0x1800b7ab0
// ============================================================================
bool RtIo_Persistence_BeginDelete(void* self, const void* nameRef) {
    return reinterpret_cast<IO::Persistence*>(self)->BeginDelete(
        *static_cast<const String*>(nameRef));
}

// ============================================================================
// bool IO::Persistence::BeginLoad(const String& name, int blockCount,
//                                 const SaveBlock* blocks)   retail 0x1800b7970
// ============================================================================
bool RtIo_Persistence_BeginLoad(void* self, const void* nameRef,
                                int blockCount, const void* blocks) {
    return reinterpret_cast<IO::Persistence*>(self)->BeginLoad(
        *static_cast<const String*>(nameRef), blockCount,
        static_cast<const SaveBlock*>(blocks));
}

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
// `??1Persistence@IO@@QEAA@XZ` -- the door for a REAL C++ destructor now. The
// explicit `->~Persistence()` is the whole delegation; nothing else destroys an
// `IO::Persistence`, because nothing in the tree ever holds one by value (the
// live object is the singleton, constructed by retail's still-unported
// 0x1800b8020 through engine/singleton_ctors.cpp).
void RtIo_Persistence_dtor(void* self) {
    reinterpret_cast<IO::Persistence*>(self)->~Persistence();
}

// ============================================================================
// `IO::Persistence`'s member bodies for this file
// ============================================================================
// Defined in `namespace IO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2). The file-local
// helpers live in this translation unit's GLOBAL anonymous namespace, so they
// are reached as `::EnqueueDelete` / `::EnqueueLoad` / `::WorkerDtor` /
// `::MapClear` -- spelled with the `::` on purpose, so a future reader cannot
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

    // Format is reached by NAME through the port-registry-aware door (already
    // ported -- see the file banner), so it still fills a raw hidden-return-
    // slot. The value lands in a POD and is then stolen into the class.
    RtSystem::String tmpPod{nullptr};
    RT_EXPORT_ONCE(::RtPersistFormatFn, "?Format@String@System@@SA?AV12@PEBDZZ")(
        &tmpPod, ::kRtPersistSaveFilenameFormat, _savePath.data, name->data);
    System::String tmp(std::move(::Sys(tmpPod)));

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
    ::EnqueueDelete()(&_workerVtable, &fn.AsString());
    // ~fn is retail's release.
    return true;
}

bool Persistence::BeginLoad(const RtSystem::String& nameRef, int blockCount,
                            const RtIo::SaveBlock* blocks) {
    const RtSystem::String* const name = &nameRef;

    if (_flags & 1u) return false;

    // Same shape as BeginDelete above.
    System::Filename fn(GetSaveFilename(*name));
    ::EnqueueLoad()(&_workerVtable, &fn.AsString(), blockCount, blocks);
    // ~fn is retail's release.
    return true;
}

Persistence::~Persistence() {

    // `_savePath` is a `RtSystem::String` member and stays one (the header says
    // why): retail's release is spelled as the String destructor through the
    // bridge, BY HAND, exactly here. Retyping it would add an implicit member
    // destructor running in ADDITION to this line.
    ::Sys(_savePath).~String();
    ::WorkerDtor()(&_workerVtable);
    ::MapClear()(this);
}

}  // namespace IO

// ---------------------------------------------------------------------------
int RtIoPersistenceSavesWinRegister();   // persistence_saves_win.cpp

int RtIoPersistenceSavesRegister() {
    int n = 0;

    RegisterRuntimeExport("?GetSaveCount@Persistence@IO@@QEBAHXZ",
                          (void*)&RtIo_Persistence_GetSaveCount); ++n;
    RegisterRuntimeExport("?GetSaveInfo@Persistence@IO@@QEBA?AUSaveGameInfo@12@H@Z",
                          (void*)&RtIo_Persistence_GetSaveInfo); ++n;
    RegisterRuntimeExport(
        "?GetSaveFilename@Persistence@IO@@AEBA?AVFilename@System@@AEBVString@4@@Z",
        (void*)&RtIo_Persistence_GetSaveFilename); ++n;
    RegisterRuntimeExport("?BeginDelete@Persistence@IO@@QEAA_NAEBVString@System@@@Z",
                          (void*)&RtIo_Persistence_BeginDelete); ++n;
    RegisterRuntimeExport(
        "?BeginLoad@Persistence@IO@@QEAA_NAEBVString@System@@HPEBUSaveBlock@12@@Z",
        (void*)&RtIo_Persistence_BeginLoad); ++n;
    RegisterRuntimeExport("??1Persistence@IO@@QEAA@XZ",
                          (void*)&RtIo_Persistence_dtor); ++n;

    // AND by address. A name-only registration leaves every ResolveAddr door
    // silently on the DLL, and ResolveAddr(Module::Runtime, ...) is
    // registry-first, so the two registrations are not redundant
    // (reference_resolveaddr_consults_port_registry_first.md).
    RegisterRuntimeAddr(0x1800b7c60, (void*)&RtIo_Persistence_GetSaveCount);
    RegisterRuntimeAddr(0x1800b7bd0, (void*)&RtIo_Persistence_GetSaveInfo);
    RegisterRuntimeAddr(0x1800b7c80, (void*)&RtIo_Persistence_GetSaveFilename);
    RegisterRuntimeAddr(0x1800b7ab0, (void*)&RtIo_Persistence_BeginDelete);
    RegisterRuntimeAddr(0x1800b7970, (void*)&RtIo_Persistence_BeginLoad);
    RegisterRuntimeAddr(0x1800b7fb0, (void*)&RtIo_Persistence_dtor);

    n += RtIoPersistenceSavesWinRegister();

    printf("[RtPort] IO::Persistence saves: %d/7 exports registered "
           "(6 here + 1 in the _win twin), each by name AND address\n", n);
    return n;
}
