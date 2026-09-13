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
#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../system/string.h"

using RtSystem::String;
using RtSystem::StringRefCount;
using RtIo::Persistence;
using RtIo::SaveGameInfo;
using RtIo::SaveBlock;

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---- unexported DLL internals, by address ----------------------------------
using EnqueueDeleteFn = void (*)(void* worker, const String* filename);
using EnqueueLoadFn   = void (*)(void* worker, const String* filename,
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
using RtPersistAdjustDelimitersFn = void  (*)(void* self);

// Retail's own format string, read out of .rdata at 0x180155840.
const char kRtPersistSaveFilenameFormat[] = "%sSave\\%s.sav";

// ============================================================================
// int IO::Persistence::GetSaveCount() const                  retail 0x1800b7c60
// ----------------------------------------------------------------------------
// The whole body is four instructions:
//   testb $1,0x18(rcx) ; je L ; xor eax,eax ; ret ; L: mov 0x88(rcx),eax ; ret
// ============================================================================
int RtIo_Persistence_GetSaveCount(const void* self) {

    auto* p = static_cast<const Persistence*>(self);
    int ours = (p->flags & 1u) ? 0 : p->saveCount;

    return ours;
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

    auto* p   = static_cast<const Persistence*>(self);
    auto* ret = static_cast<SaveGameInfo*>(retval);

    if (p->flags & 1u) {
        ret->sortKey   = 0;
        ret->name.data = nullptr;
    } else {
        const SaveGameInfo& e = p->saves[index];
        ret->sortKey   = e.sortKey;
        ret->name.data = nullptr;               // retail zeroes it first, then AddRefs
        if (e.name.data) {
            ret->name.data = e.name.data;
            ++StringRefCount(ret->name.data);
        }
    }

    return retval;
}

// ============================================================================
// System::Filename IO::Persistence::GetSaveFilename(const String&) const
// retail 0x1800b7c80 — PRIVATE (`AEBA`), and exported all the same: this DLL
// exports its private members, so it is registered BY NAME like any other row
// (reference_aeaa_means_private_not_unexported.md). Also returns BY VALUE:
// (this=RCX, hiddenSlot=RDX, const String& name=R8).
// ============================================================================
void* RtIo_Persistence_GetSaveFilename(const void* self, void* retval, const void* nameRef) {

    auto* p    = static_cast<const Persistence*>(self);
    auto* ret  = static_cast<String*>(retval);      // Filename IS a String (Filename.h)
    auto* name = static_cast<const String*>(nameRef);

    String tmp;
    tmp.data = nullptr;
    RT_EXPORT_ONCE(RtPersistFormatFn, "?Format@String@System@@SA?AV12@PEBDZZ")(
        &tmp, kRtPersistSaveFilenameFormat, p->savePath.data, name->data);

    ret->data = nullptr;
    if (tmp.data) {
        ret->data = tmp.data;
        ++StringRefCount(ret->data);
    }
    RT_EXPORT_ONCE(RtPersistAdjustDelimitersFn,
                   "?AdjustDelimiters@Filename@System@@AEAAXXZ")(ret);
    RtSystem::RtString_ReleaseInline(&tmp);

    return retval;
}

// ============================================================================
// bool IO::Persistence::BeginDelete(const String& name)      retail 0x1800b7ab0
// ============================================================================
bool RtIo_Persistence_BeginDelete(void* self, const void* nameRef) {

    auto* p = static_cast<Persistence*>(self);
    if (p->flags & 1u) return false;

    String fn;
    fn.data = nullptr;
    RtIo_Persistence_GetSaveFilename(self, &fn, nameRef);
    EnqueueDelete()(&p->workerVtable, &fn);
    RtSystem::RtString_ReleaseInline(&fn);
    return true;
}

// ============================================================================
// bool IO::Persistence::BeginLoad(const String& name, int blockCount,
//                                 const SaveBlock* blocks)   retail 0x1800b7970
// ============================================================================
bool RtIo_Persistence_BeginLoad(void* self, const void* nameRef,
                                int blockCount, const void* blocks) {

    auto* p = static_cast<Persistence*>(self);
    if (p->flags & 1u) return false;

    String fn;
    fn.data = nullptr;
    RtIo_Persistence_GetSaveFilename(self, &fn, nameRef);
    EnqueueLoad()(&p->workerVtable, &fn, blockCount,
                  static_cast<const SaveBlock*>(blocks));
    RtSystem::RtString_ReleaseInline(&fn);
    return true;
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
void RtIo_Persistence_dtor(void* self) {

    auto* p = static_cast<Persistence*>(self);
    RtSystem::RtString_ReleaseInline(&p->savePath);
    WorkerDtor()(&p->workerVtable);
    MapClear()(self);
}

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
