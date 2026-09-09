// ============================================================================
// runtime/src/ui/dialog.cpp — Wave 7 lane B
//   UI::DialogInstance + UI::Dialog + UI::DialogController + UI::ElementDescription
// ============================================================================
//
// Ports 14 of the 28 lane-B symbols (PLAN.md §4.2 wave 7, §6). The recovered
// layouts and the evidence for every offset are in runtime/types/ui/
// DialogInstance.h, Dialog.h and ElementDescription.h; the per-function
// write-up, the oracle argument for each, everything held back and why, and
// the resolution of this lane's two phantom export names are in
// docs/runtime_port/WAVE7_LANE_B.md.
//
//   | Export (short form)                   | Retail      | Ord  | Ported | Oracle |
//   |---------------------------------------|-------------|------|--------|--------|
//   | ??0DialogInstance (ctor)              | 0x180114870 |  147 | NO     | —      |
//   | ??1DialogInstance (dtor)              | 0x180114d40 |  574 | W9-UID | shape  |
//   | ??_7DialogInstance (vtable)           | 0x180156d70 | 1420 | n/a    | —      |
//   | ?AddController@ (I,H,H,External*,Str) |  NOT AN EXPORT     | n/a    | —      |
//   | ?AddController@ (…ExtPtr,ExtPtr,Str)  | 0x180113470 | 1541 | NO     | —      |
//   | ?AddController@ (…ExtPtr,IntPtr)      | 0x1801133b0 | 1542 | NO     | —      |
//   | ?FindElement@                         | 0x180114c70 | 2346 | yes    | replay |
//   | ?FindElementType@                     | 0x1801140c0 | 2348 | yes    | replay |
//   | ?GetController@                       | 0x180113320 | 2513 | yes    | replay |
//   | ?GetRoot@                             | 0x180045580 | 2875 | yes    | replay |
//   | ?OnElementEvent@                      | 0x180114ed0 | 3274 | yes    | shape  |
//   | ?OnUpdate@                            | 0x180115400 | 3286 | yes    | shape  |
//   | ?OnUpdateLayout@                      | 0x1801153a0 | 3289 | yes    | shape  |
//   | ?SetFlagsOnOff@                       | 0x180114690 | 3882 | yes    | replay |
//   | ?SetUpperLeft@                        | 0x180114a60 | 3997 | NO     | —      |
//   | ?Persist@DialogInstance@              | 0x180114fa0 | 3322 | W9-Z   | none   |
//   | ?OnSerializeSettings@DialogInstance@  | 0x180115470 | 3281 | W9-Z   | none   |
//   | ?TypeName@DialogInstance@             | 0x180045400 | 4379 | W9-Z   | none   |
//   | ?OnClosed@DialogInstance@             | 0x180115330 | 3269 | NO(*)  | —      |
//     (*) DECLINED AGAIN by WAVE 9 LANE UID, on a blocker RE-MEASURED rather
//         than inherited — see the block below RtUiDialogInstance_Dtor.
//   | ?StartTimer@                          | 0x1801137c0 | 4081 | NO     | —      |
//   | ?StopTimer@                           | 0x180113730 | 4111 | yes    | replay |
//   | ?CreateElement@Dialog@                | 0x180112ed0 | 1888 | NO     | —      |
//   | ?GetReference@Dialog@ (as spelled)    |  NOT AN EXPORT     | n/a    | —      |
//   | ?GetTemplate@Dialog@                  | 0x1800388b0 | 2956 | yes    | replay |
//   | ?_typeInfo@Dialog@ (data)             | 0x1801faa88 | 4847 | n/a    | —      |
//   | ??1DialogController (dtor)            | 0x1800cd2f0 |  572 | yes    | shape  |
//   | ?GetContainer@DialogController@       | 0x18003b700 | 2508 | yes    | replay |
//   | ??0ElementDescription (ctor)          | 0x180115a40 |  173 | yes    | replay |
//   | ??1ElementDescription (dtor)          | 0x180116100 |  585 | yes(*) | none   |
//     (*) WAVE 9 LANE UIF. `none` is the literal oracle disposition, not a gap
//         nobody noticed: a replay would double-release a refcounted String and
//         run ~External twice. See the body's header near the bottom of this file.
//   | ?CreateInstance@ElementDescription@   | 0x180115ad0 | 2004 | W10-UIF| replay+twin |
//   | ?GetFileVersion@ElementDescription@   | 0x180116af0 | 2624 | yes    | replay |
//   | ?Serialize@ElementDescription@        | 0x180116b00 | 3687 | NO     | —      |
//
// All 28 mangled names were checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body was
// written (reference_resolveexport_phantom_names.md: a name the DLL does not
// contain resolves to NULL in silence, the port still registers, and the
// census still counts it). TWO ARE NOT THERE — see the next block. The
// addresses and ordinals in the table are that query's output, not Ghidra's.
//
// ---------------------------------------------------------------------------
// THE TWO PHANTOM NAMES — BOTH ARE LIVE DEFECTS, NEITHER IS PORT WORK
// ---------------------------------------------------------------------------
// ?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z
//     Not in the export table. It is a mis-mangling of the SIX-argument
//     overload that IS there, 0x180113470 / ordinal 1541, whose real signature
//     is (uint, Base*, ExternalPtr<DialogControllerConfig>,
//     ExternalPtr<ElementDescription>, String const&). The phantom spells
//     arguments 2 and 3 as plain `int`s, which is the
//     reference_hidden_retval_and_int_pointer_args.md failure exactly: the two
//     pointer arguments land in R8/R9 as 32-bit halves.
//     `engine_funcs.cpp` ALREADY resolves that same overload correctly as
//     `fn_DialogInstance_AddController6`, so the phantom is a duplicate — and
//     its ResolveExport returns null, at which point its fallback reads exe
//     dispatch slot 0x140151a60, which `tools/dispatch_slot_map.py` says is
//     `?AddChild@Element@UI@@…`. So `fn_DialogInstance_AddController` has been
//     silently bound to Element::AddChild. It is harmless ONLY because the
//     pointer has zero callers tree-wide; it is removed at its source rather
//     than repaired (PLAN §6 step 8), and WAVE7_LANE_B.md §5 records it.
//
// ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ
//     Not in the export table, and neither is any symbol containing
//     "DialogReference" — the class name was invented. The real function is
//     ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VExternal@Object@@@Object@@H@Z
//     at 0x1800388d0 / ordinal 2861: it returns ExternalPtr<Object::External>
//     and it TAKES AN INT INDEX, which the phantom typedef omits. It sits four
//     bytes after ?GetTemplate@ and is the identical one-liner against a
//     different array (Dialog+0x78 vs +0x68). `fn_Dialog_GetReference` has no
//     fallback, so it has been NULL since it was written, and no caller has
//     ever noticed because it has no callers. Removed at its source too.
//
// ---------------------------------------------------------------------------
// WHY REGISTERING THIS PORT IS SAFE
// ---------------------------------------------------------------------------
// Registering redirects OUR callers only. The DLL keeps calling its own
// bodies through direct near-CALLs the registry cannot see, and — new in this
// lane — three of the fourteen ports are VIRTUALS (`?OnElementEvent@`,
// `?OnUpdate@`, `?OnUpdateLayout@` are all `UEAA`), so every dispatch through
// the DLL's own `??_7DialogInstance@UI@@6B@` still lands in the DLL no matter
// what we register. That is not a problem to fix, it is the shape of the
// wave: what we redirect is the `fn_DialogInstance_*` pointers our exe-side
// dialog vtables call directly.
//
// It is safe here because of four facts, each checked rather than assumed:
//
//   1. ⚠ NO LONGER TRUE AS WRITTEN — corrected by WAVE 9 LANE UID rather than
//      left standing. Wave 7's claim was: nothing ported here allocates,
//      frees or reshapes a tree; every body reads a field, writes fields of
//      an object somebody else owns at static_assert'ed offsets, or walks an
//      existing array. That still describes every Wave 7 body, and it is why
//      the replay oracle is sound for them.
//      IT DOES NOT DESCRIBE `??1DialogInstance@UI@@` (lane UID). That body
//      FREES two arrays, destroys a whole child tree through slot 0, clears
//      a linked list and removes the object from the handle table. It is
//      therefore NOT covered by argument 3 below either: it has no replay
//      oracle and cannot have one. What carries it instead is stated at the
//      body — a shape check of the six values that could silently be wrong,
//      compared against the shipped bytes while the DLL is still present.
//      Everything else that allocates (both AddControllers, StartTimer,
//      CreateInstance, the two ctors) is still held back — WAVE7_LANE_B.md §2.
//   2. NO PORTED BODY STAMPS A VTABLE IT MINTED. The one ported function that
//      writes vtable pointers, `??0ElementDescription`, resolves all three of
//      them (`??_7Base@Object@@6B@`, `??_7External@Object@@6B@`,
//      `??_7ElementDescription@UI@@6B@`) out of the DLL's own export table and
//      stores those. This is the `??_7Geometry@Graphics@@6B@` lever from Wave
//      6 and it is what keeps a half-ported class from putting two vtables in
//      play (PLAN §4.3 risk 3).
//   3. THE REPLAY ORACLE PROVES, ON EVERY DIFFED CALL, that our body leaves
//      the object byte-identical to what the DLL's body would have left. That
//      is what makes "two implementations, one live object" a non-event.
//   4. NO DETOUR IS INSTALLED, deliberately, for the same reason Wave 6 lane A
//      installed none: it would route the DLL's own UI loop into this port for
//      no gain the argument above does not already give.
//
// ---------------------------------------------------------------------------
// ONE REGISTRATION HAS A WIDER BLAST RADIUS THAN THE OTHERS — AND IT IS HELD BACK
// ---------------------------------------------------------------------------
// `standalone/address_registry.cpp` binds five exe PE-import slots to lane-B
// exports by NAME, through `RegisterDispatchSlot(slot, ResolveExport(name))`:
//
//     0x1401518b0  ??0ElementDescription@UI@@QEAA@XZ
//     0x1401518b8  ??1ElementDescription@UI@@UEAA@XZ
//     0x1401518c0  ?GetFileVersion@ElementDescription@UI@@UEBAHXZ
//     0x1401518c8  ?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z
//     0x1401518d0  ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@…@Z
//     0x1401528c0  ?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@…@Z
//
// `RuntimePortInit()` runs BEFORE `AddressRegistryInit()` (standalone/main.cpp
// :1614 / :1626, and the comment there says so on purpose), and `ResolveExport`
// consults the runtime registry first — so registering one of those names does
// not merely redirect our own `fn_*` pointers, it re-points the exe's own
// retail-shaped `.rdata` slot at our body too. For the two we do register
// (`??0ElementDescription`, `?GetFileVersion@`) that is exactly what we want
// and both are fully oracled. For `?Serialize@` it would put our body on the
// UIDescription parse path of every `.rsc` in the game, which is the single
// worst place in this codebase to land an unverifiable port
// (reference_createplaced_placebitarray_zeroed_by_reparse.md, and commit
// a12037d). It is held back; see WAVE7_LANE_B.md §2.
//
// ---------------------------------------------------------------------------
// THREE ORACLES
// ---------------------------------------------------------------------------
// (a) REPLAY, for the accessors and the two self-contained mutators. Snapshot
//     the live object into a scratch copy BEFORE our body runs, run our body
//     on the real object, then run the DLL's body on the scratch copy and
//     compare. Because the scratch is a memcpy of the live object, the four
//     padding holes the DialogInstance ctor never initialises (DialogInstance.h
//     lists them) hold identical bytes on both sides — this is NOT the
//     "comparing bytes neither side wrote" trap
//     (reference_oracle_must_not_compare_unwritten_bytes.md), which bites only
//     when one side starts from fill.
//     For the const accessors the comparison is the returned value, which is
//     the entire output.
//
// (b) REPLAY WITH A REDIRECTED ARRAY, for `?StopTimer@` alone. It mutates a
//     heap buffer the object merely points at, so a naive replay would shift
//     the SAME buffer twice. The scratch object's `_timers._data` is repointed
//     at a private copy of the timer entries first, and the comparison is over
//     a fingerprint of the object prefix (minus that one pointer) plus the live
//     timer entries.
//
// (c) SHAPE, decoded from the shipped bytes at registration time, for the four
//     functions a replay would double-execute: the three controller-dispatch
//     loops (running them twice would tick every controller twice) and
//     `??1DialogController` (running it twice would swap-remove the object from
//     the handle table twice). A dispatch loop's ENTIRE failure surface is
//     three numbers — which field holds the array, which field holds the count,
//     and which vtable slot it calls — and all three are literal displacements
//     in the shipped instruction stream. `VerifyDispatchLoop` reads them out
//     and reports through an ordinary diff site. This is the Wave 6 lane A
//     `VerifyThunk` idea applied to a loop instead of a tail-jump.
//     ~DialogController's whole body is two instructions (stamp a vtable, tail-
//     jump to `Object::Internal::~Internal`), and `VerifyDtorThunk` checks both
//     of the addresses they name.
//     A shape check appears in `RtDiffSummary()` as one call, not per
//     invocation — say so when reading the summary rather than concluding the
//     function was never exercised.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "dialog_vptr_trip.h"   // WAVE 10E lane FINAL
#include "../../types/ui/Dialog.h"
#include "../../types/ui/DialogInstance.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/ui/StringTable.h"   // WAVE 10 LANE UIF — the twin's table
// ⚠ WAVE 9 LANE Z — `runtime/types/object/TypeInfo.h` IS NOT INCLUDABLE HERE,
// and the reason is a real defect in another lane's header rather than anything
// about this file. `runtime/types/object/External.h` opens `namespace RtObject
// {` at its line 37 and then `#include "TypeInfo.h"` at its line 276, INSIDE
// that namespace — so every declaration in TypeInfo.h lands in
// `RtObject::RtObject`, and `#pragma once` means whichever translation unit
// pulls External.h first (here, via ElementDescription.h) fixes that nesting for
// the whole TU. `RtObject::BaseTypeInfo` then does not name a type.
//
// Filed, NOT fixed: moving that one `#include` out of External.h's namespace is
// a two-line change in a header 40+ files depend on, it is not this lane's file,
// and it cannot be validated without a full build — which lane Z does not hold.
// `?TypeName@DialogInstance@` below therefore spells the one offset it needs as
// a raw offset with its evidence at the use site, which is the convention
// runtime/types/ui/UserInterface.h already states for foreign offsets.

using RtUI::Dialog;
using RtUI::DialogController;
using RtUI::DialogInstance;
using RtUI::DialogTimer;
using RtUI::ElementDescription;
using RtUI::kDialogInstanceCtorExtent;
using RtUI::DialogNamedTemplate;   // WAVE 9 LANE UIC (?CreateElement@Dialog@)

// ----------------------------------------------------------------------------
// ResolveExport, forward-declared rather than pulled in from standalone/resolve.h
// ----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring RegisterRuntimeExport locally:
// resolve.h drags in <windows.h>, whose <wingdi.h> declares a function called
// `Rectangle` and whose <windef.h> still #defines `near`/`far` away, and
// PLAN.md §4.1 decision 1 requires runtime/src/ to stay platform-neutral.
// This declaration must stay byte-identical to resolve.h's; the linker checks
// the two against each other. (Precedent: runtime/src/video/context.cpp and
// runtime/src/achievement/achievement_manager.cpp declare the same prototype.)
void* ResolveExport(const char* mangled_name);

// WAVE 10 LANE UIF negative-control knob; defined in runtime/src/ui/userinterface.cpp.
extern "C" int RtUiUifNegCtrl();

// ResolveAddr, same reasoning and the same "must stay byte-identical to
// resolve.h's" contract. WAVE 9 LANE UIC needs it for one unexported-by-us edge
// (`?AdjustUpperLeft@Element@UI@@`, reached through the seam so that
// BANISHED_RT_UIELEMENT=0 puts it back on the DLL).
//
// AND IT IS NOT "ALWAYS THE DLL": ResolveAddr(Module::Runtime, …) consults the
// port registry FIRST (standalone/resolve_standalone.cpp:471,
// reference_resolveaddr_consults_port_registry_first.md). That is the point of
// using it here rather than calling the C++ symbol directly.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// WAVE 9 LANE UID. The RAW address door -- the shipped DLL and nothing else --
// needed by the two shape oracles at the bottom of the anonymous namespace,
// which must read the SHIPPED instruction stream and not our own prologue
// (reference_resolveaddr_consults_port_registry_first.md, and the thirteen
// detour installers lane FNHOOK found aimed at our own bytes). Byte-identical
// to standalone/resolve.h:108; the linker checks the two against each other.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

// ============================================================================
// Mangled names.
//
// The registrations at the bottom of this file deliberately repeat each name
// as its own string literal instead of using these constants:
// tools/runtime_census.py cannot see through a NAMED CONSTANT and would
// under-count the lane, which is how Wave 3's Object count stood at 21/41 when
// it was really 27/41 (reference_census_cannot_see_nonliteral_registrations.md).
// These constants exist only for the oracle's RT_ORIGINAL_ONCE lookups.
// ============================================================================
const char* const kGetRoot =
    "?GetRoot@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ";
const char* const kGetController =
    "?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z";
const char* const kFindElement =
    "?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z";
const char* const kFindElementType =
    "?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z";
const char* const kSetFlagsOnOff = "?SetFlagsOnOff@DialogInstance@UI@@QEAAXII@Z";
const char* const kStopTimer     = "?StopTimer@DialogInstance@UI@@QEAAXH@Z";
const char* const kGetTemplate =
    "?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z";
const char* const kGetContainer =
    "?GetContainer@DialogController@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ";
const char* const kEdCtor        = "??0ElementDescription@UI@@QEAA@XZ";
const char* const kEdFileVersion = "?GetFileVersion@ElementDescription@UI@@UEBAHXZ";
// WAVE 9 LANE UIC
const char* const kCreateElement =
    "?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@"
    "V?$ExternalPtr@VElementDescription@UI@@@4@@Z";
const char* const kSetUpperLeft  = "?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z";
// WAVE 10 LANE UIF. Oracle lookup only — the REGISTRATION at the bottom of this
// file repeats the literal, for the census reason the block above gives.
const char* const kEdCreateInstance =
    "?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z";

// The Element helpers the two Find* forwarders tail into. Neither is in this
// lane's scope; both are exported and both are resolved through the SEAM
// (ResolveExport, not ResolveExportRaw) so that if lane A registers a port of
// either, our forwarder follows it — which is what retail's direct near-CALL
// into the same DLL would have amounted to.
const char* const kElementFindElement =
    "?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z";
const char* const kElementFindElementType =
    "?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z";

// ---- Raw retail ABI typedefs ----------------------------------------------
// Every member below takes `this` in RCX. Every one whose C++ return type is a
// class (InternalPtr<T>, ExternalPtr<T>) has a HIDDEN RETURN SLOT in RDX and
// returns that same pointer in RAX — confirmed instruction by instruction in
// the disassembly, e.g. ?GetRoot@ is literally
//   MOV RAX,[RCX+0x50] / MOV [RDX],RAX / MOV RAX,RDX / RET.
// Getting this wrong is the wild-write family
// (reference_hidden_retslot_wild_write.md), so it is spelled out per typedef.
using PtrOutFn      = void** (*)(void* self, void** out);
using PtrOutU32Fn   = void** (*)(void* self, void** out, uint32_t key);
using PtrOutI32Fn   = void** (*)(void* self, void** out, int32_t index);
using PtrOutStrFn   = void** (*)(void* self, void** out, const void* str);
using SetFlagsFn    = void   (*)(void* self, uint32_t on, uint32_t off);
using StopTimerFn   = void   (*)(void* self, int32_t id);
using CtorFn        = void*  (*)(void* self);
using IntFn         = int32_t(*)(const void* self);
using TypeIdFn      = uint32_t(*)(void* controller);

// ---- WAVE 9 LANE UIC helpers ----------------------------------------------
// The `System::String` release retail open-codes at every temporary, with its
// asymmetry intact: the FREEING branch leaves the local holding `data - 8`
// (dangling, and then freed), the SHARING branch nulls it. Identical to the one
// `??1ElementDescription@` at the bottom of this file spells out; factored here
// because `?CreateElement@` performs it three times.
//
// Takes the local BY REFERENCE so the two different post-states are visible to
// the caller exactly as they are in retail — a by-value helper would look
// correct and silently drop the asymmetry.
void ReleaseStringLocal(char*& p) {
    if (!p) return;
    if (--RtSystem::StringRefCount(p) == 0) {
        p -= 8;
        using FreeFn = void (*)(void*);
        static FreeFn freeFn = reinterpret_cast<FreeFn>(
            ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
        if (freeFn) freeFn(p);
    } else {
        p = nullptr;
    }
}

// `?_singleton@Device@Video@@0PEAV12@EA` — a MINTED data slot
// (standalone/rt_data_mint_tables.inc:1565), reached through the same
// RT_DATA_SLOT door runtime/src/ui/userinterface.cpp uses rather than by a raw
// address read, so it is satisfied at cutover and never caches a null.
inline void** VideoDeviceSingleton() {
    return RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
}

// ============================================================================
// (a) The replay oracle
// ============================================================================
// File-scope scratch, not stack buffers: the UI runs on one thread (the DLL's
// DialogInstance tree is created and driven only from Engine::MainLoop's
// thread), and no ported body below calls another ported body, so a replay can
// never nest. Both buffers are touched only inside `if (rt_diff_on__)`, so a
// normal run neither pays for them nor reads them.
//
// If UI updates ever become concurrent these must become per-call storage.
alignas(16) uint8_t g_replayObj[sizeof(DialogInstance)];

// StopTimer's private copy of the timer entries, plus the two fingerprints its
// comparison is built from. 8 KB holds 341 timers; a real dialog runs a
// handful. When a dialog somehow exceeds it the call is simply NOT DIFFED —
// deliberately, rather than reported as a divergence, because "the oracle's
// buffer was too small" is not a fact about the port. WAVE7_LANE_B.md §4 says
// so, so that a `calls=` count lower than expected is not read as a stall.
constexpr size_t kReplayTimerBytes = 8192;
alignas(16) uint8_t g_replayTimers[kReplayTimerBytes];
uint8_t g_fpOurs[kDialogInstanceCtorExtent + kReplayTimerBytes];
uint8_t g_fpTheirs[kDialogInstanceCtorExtent + kReplayTimerBytes];

// WAVE 10 LANE UIF. Defined near the bottom of this file. It is triggered from
// the FIRST `??0ElementDescription` and NOT from `RtUiDialogRegister()`, for the
// reason runtime/src/ui/userinterface.cpp measured the hard way: registration
// runs before `Engine::Startup` creates `System::MemoryAllocator::_singleton`,
// and any oracle that allocates its own subjects faults there with
// `WRITE of 0x648` (retail's `EnterCriticalSection(_singleton + 0x640)` on a
// null singleton).
void VerifyElementDescriptionCreateInstanceTwin();

inline DialogInstance* ReplaySnapshot(bool on, const DialogInstance* live) {
    if (!on) return nullptr;
    std::memcpy(g_replayObj, live, sizeof(DialogInstance));
    return reinterpret_cast<DialogInstance*>(g_replayObj);
}

// One pointer comparison, reported through an ordinary diff site. Written out
// rather than using RT_DIFF_SCALAR because that macro evaluates its OURS
// argument twice, and every use below would then call the original twice.
inline void DiffPtr(RtDiffSite* site, const void* ours, const void* theirs) {
    char a[32], b[32];
    RtDiffFormat(a, sizeof(a), ours);
    RtDiffFormat(b, sizeof(b), theirs);
    RtDiffReport(site, ours == theirs, a, b);
}

// ============================================================================
// (c) The shape oracle
// ============================================================================
// The three controller-dispatch loops all compile to the same five-instruction
// core, and the only things that can be wrong about our transcription of one
// are the three displacements in it:
//
//   39 99 <disp32>     CMP  dword ptr [RCX+disp32],EBX     ; the COUNT field
//   48 8b 46 <disp8>   MOV  RAX,qword ptr [RSI+disp8]      ; the ARRAY field
//   ff 50 <disp8>      CALL qword ptr [RAX+disp8]          ; the VTABLE SLOT
//
// (RSI is the `this` the prologue parked there; RCX still holds it at the CMP.)
// Get the array or count offset wrong and the loop walks the timer array
// instead; get the slot wrong and every controller gets a different virtual
// called with this method's arguments. Decoding all three out of the shipped
// bytes covers all of it.
struct LoopShape {
    bool     ok;
    uint32_t countOffset;
    uint32_t arrayOffset;
    uint32_t slotOffset;
};

// Find `pattern` (len bytes) in [p, p+span), returning the index or -1.
int FindBytes(const uint8_t* p, size_t span, const uint8_t* pattern, size_t len) {
    if (!p) return -1;
    for (size_t i = 0; i + len <= span; ++i)
        if (std::memcmp(p + i, pattern, len) == 0) return static_cast<int>(i);
    return -1;
}

LoopShape DecodeDispatchLoop(const uint8_t* p) {
    LoopShape s = { false, 0, 0, 0 };
    if (!p) return s;
    // The longest of the three bodies (?OnElementEvent@) reaches its CALL at
    // +0x8e, so 0xc0 is a comfortable window that still cannot wander into the
    // next function's body far enough to matter: the three patterns must be
    // found IN ORDER, each after the previous one.
    const size_t span = 0xc0;

    // CMP dword ptr [RCX+disp32],EBX — match opcode+modrm, read the disp32.
    static const uint8_t kCmpOp[2] = { 0x39, 0x99 };
    const int iCmp = FindBytes(p, span, kCmpOp, sizeof(kCmpOp));
    if (iCmp < 0) return s;
    std::memcpy(&s.countOffset, p + iCmp + 2, 4);

    static const uint8_t kMovOp[3] = { 0x48, 0x8b, 0x46 };  // MOV RAX,[RSI+disp8]
    const int iMov = FindBytes(p + iCmp, span - static_cast<size_t>(iCmp), kMovOp, sizeof(kMovOp));
    if (iMov < 0) return s;
    s.arrayOffset = p[iCmp + iMov + 3];

    static const uint8_t kCallOp[2] = { 0xff, 0x50 };       // CALL [RAX+disp8]
    const size_t base = static_cast<size_t>(iCmp) + static_cast<size_t>(iMov);
    const int iCall = FindBytes(p + base, span - base, kCallOp, sizeof(kCallOp));
    if (iCall < 0) return s;
    s.slotOffset = p[base + static_cast<size_t>(iCall) + 2];

    s.ok = true;
    return s;
}

void VerifyDispatchLoop(RtDiffSite* site, const char* mangled, unsigned expectedSlot) {
    if (!RtDiffEnabled()) return;
    RtDiffRegister(site);

    const uint8_t* body = static_cast<const uint8_t*>(ResolveExportRaw(mangled));
    const LoopShape f = DecodeDispatchLoop(body);

    char ours[96], theirs[128];
    std::snprintf(ours, sizeof(ours), "count+0x%x array+0x%x slot+0x%x",
                  (unsigned)offsetof(DialogInstance, _controllers._size),
                  (unsigned)offsetof(DialogInstance, _controllers._data),
                  expectedSlot);
    if (!f.ok) {
        std::snprintf(theirs, sizeof(theirs), "unrecognised loop shape at %p", (const void*)body);
        RtDiffReport(site, false, ours, theirs);
        return;
    }
    std::snprintf(theirs, sizeof(theirs), "count+0x%x array+0x%x slot+0x%x",
                  f.countOffset, f.arrayOffset, f.slotOffset);
    const bool equal =
        (f.countOffset == offsetof(DialogInstance, _controllers._size)) &&
        (f.arrayOffset == offsetof(DialogInstance, _controllers._data)) &&
        (f.slotOffset  == expectedSlot);
    RtDiffReport(site, equal, ours, theirs);
}

// ~DialogController is two instructions of substance:
//   48 8d 05 <rel32>   LEA RAX,[rip+rel32]      ; ??_7DialogController@UI@@6B@
//   48 89 01           MOV [RCX],RAX
//   ...
//   e9 <rel32>         JMP  Object::Internal::~Internal
// Both targets are exported, so both can be checked by name.
void VerifyDtorThunk(RtDiffSite* site, const char* mangled,
                     const void* expectedVtable, const void* expectedTail) {
    if (!RtDiffEnabled()) return;
    RtDiffRegister(site);

    const uint8_t* p = static_cast<const uint8_t*>(ResolveExportRaw(mangled));
    char ours[96], theirs[128];
    std::snprintf(ours, sizeof(ours), "vtbl=%p tail=%p", expectedVtable, expectedTail);

    static const uint8_t kLea[3] = { 0x48, 0x8d, 0x05 };
    const int iLea = FindBytes(p, 0x40, kLea, sizeof(kLea));
    static const uint8_t kJmp[1] = { 0xe9 };
    const int iJmp = FindBytes(p, 0x40, kJmp, sizeof(kJmp));
    if (!p || iLea < 0 || iJmp < 0) {
        std::snprintf(theirs, sizeof(theirs), "unrecognised dtor shape at %p", (const void*)p);
        RtDiffReport(site, false, ours, theirs);
        return;
    }
    int32_t leaRel = 0, jmpRel = 0;
    std::memcpy(&leaRel, p + iLea + 3, 4);
    std::memcpy(&jmpRel, p + iJmp + 1, 4);
    const void* vtbl = p + iLea + 7 + leaRel;
    const void* tail = p + iJmp + 5 + jmpRel;
    std::snprintf(theirs, sizeof(theirs), "vtbl=%p tail=%p", vtbl, tail);
    RtDiffReport(site, vtbl == expectedVtable && tail == expectedTail, ours, theirs);
}

// ---- Wave 9 lane Z: the exported DATA slot ?TypeName@ reads ----------------
// Through RT_DATA_SLOT, which caches a hit and RETRIES a miss, so the row can
// be minted after this file first resolves it. See the ⚠ note on
// RtUiDialogInstance_TypeName below: this row is NOT in the mint today.
inline const uint8_t* DialogInstanceTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@DialogInstance@UI@@0VBaseTypeInfo@Object@@A");
}

// `Object::BaseTypeInfo::name`. The layout is runtime/types/object/TypeInfo.h's
// (+0x00 typeId, +0x04 an unused word, +0x08 `const char* name`, 0x38 total);
// it is spelled as a constant here only because that header cannot be included
// in this TU — see the ⚠ note beside the includes.
constexpr int kBaseTypeInfoName = 0x08;


// ---- WAVE 9 LANE UID helpers ----------------------------------------------
// All four use the RETRY-a-miss form (RT_DATA_SLOT / RT_EXPORT_ONCE, and the
// hand-written equivalent for the one address door), not the frozen
// function-local static this file uses elsewhere. That is lane E3's finding
// applied rather than repeated: 22 sites in the tree let the FIRST caller fix
// a null for the life of the process, and `~DialogInstance` runs during
// teardown, which is exactly when an ordering assumption is least safe.

using DeletingDtorFn = void* (*)(void* self, unsigned flags);
using VoidPtrFn      = void  (*)(void* p);

inline void* VtblDialogInstance() {
    return RT_DATA_SLOT(void*, "??_7DialogInstance@UI@@6B@");
}

// Slot 0 of any Object::Base-derived vtable is the SCALAR DELETING DESTRUCTOR
// (runtime/src/object/deleting_dtor.cpp). Retail calls it with EDX = 1, which
// means "destruct AND free"; see the note in the block below.
inline DeletingDtorFn DeletingDtorOf(void* obj) {
    return reinterpret_cast<DeletingDtorFn>((*reinterpret_cast<void***>(obj))[0]);
}

// 0x1800c7ad0, which is the out-of-line spelling of the lock+Free retail
// inlines at both of this dtor's array frees. Same argument
// runtime/src/collection/linkedlist.cpp's header block makes for 0x180009c30.
inline void MemFreeStatic(void* p) {
    RT_EXPORT_ONCE(VoidPtrFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// 0x1800325f0 — Collection::LinkedList<T=8>::Clear. UNEXPORTED, so the only
// door is the address one; port-aware, so `BANISHED_RT_LINKEDLIST=0` still
// routes this to the DLL exactly as it does that file's other two bodies.
inline void ListClear(void* list) {
    static VoidPtrFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<VoidPtrFn>(ResolveAddr(Module::Runtime, 0x1800325f0));
    if (fn) fn(list);
}

// ??1Internal@Object@@UEAA@XZ — the tail retail inlines. Reached by NAME
// through the port registry, which is precisely what RtUiDialogController_Dtor
// already does for the identical tail.
inline void InternalDtor(void* self) {
    RT_EXPORT_ONCE(VoidPtrFn, "??1Internal@Object@@UEAA@XZ")(self);
}

// ---- the two shape oracles ------------------------------------------------
//
// Both read the SHIPPED instruction stream through a RAW door and compare its
// literal displacements and relocation targets against what the ports assume.
// Raw is mandatory here and is not an oversight of the port-aware rule: an
// oracle that resolved through the registry would read OUR OWN prologue once
// the port registers and compare the port against itself
// (WAVE9_LANE_FNHOOK.md found thirteen detour installers doing exactly that).
// Both are called only from RtUiDialogRegister and only when the oracle is on,
// so neither is a live call site at cutover.

// Little-endian rel32 at p[+off], resolved against the end of the instruction.
inline const uint8_t* Rel32Target(const uint8_t* p, int off, int instrLen) {
    int32_t rel = 0;
    std::memcpy(&rel, p + off, 4);
    return p + off + (instrLen - off) + rel;
}

// ??1DialogInstance@UI@@UEAA@XZ, 0x180114d40. Six values, and for a destructor
// whose every other operation is a store of zero they ARE the failure surface:
// the three vtables it stamps, the two functions it calls, and the field it
// hands to the first of them.
void VerifyDialogInstanceDtorShape(RtDiffSite* site) {
    if (!RtDiffEnabled()) return;
    RtDiffRegister(site);

    // RT_RAW_OK: an ORACLE read of the shipped bytes; guarded by RtDiffEnabled
    // above and never reached on a normal run.
    const uint8_t* p = static_cast<const uint8_t*>(
        ResolveExportRaw("??1DialogInstance@UI@@UEAA@XZ"));

    char ours[224], theirs[224];
    // RT_RAW_OK: oracle expectations, read raw for the same reason.
    const void* wantVtDlg  = ResolveExportRaw("??_7DialogInstance@UI@@6B@");
    const void* wantVtInt  = ResolveExportRaw("??_7Internal@Object@@6B@");
    const void* wantVtBase = ResolveExportRaw("??_7Base@Object@@6B@");
    const void* wantClear  = reinterpret_cast<const void*>(ResolveRuntimeRaw(0x1800325f0));
    const void* wantSetSz  = reinterpret_cast<const void*>(ResolveRuntimeRaw(0x18002fca0));
    const unsigned wantChildren =
        static_cast<unsigned>(offsetof(DialogInstance, _children));

    std::snprintf(ours, sizeof(ours), "vt=%p/%p/%p call=%p/%p children+0x%x",
                  wantVtDlg, wantVtInt, wantVtBase, wantClear, wantSetSz, wantChildren);

    if (!p) {
        std::snprintf(theirs, sizeof(theirs), "shipped body not resolvable");
        RtDiffReport(site, false, ours, theirs);
        return;
    }

    // Scan the 0x18a-byte body once, collecting the LEA RAX,[rip+d32] targets
    // (48 8d 05), the direct CALL rel32 targets (e8) and the LEA RCX,[RDI+d8]
    // (48 8d 4f d8) that computes the argument to the third call.
    const void* lea[4]  = { nullptr, nullptr, nullptr, nullptr };
    const void* call[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    int nLea = 0, nCall = 0;
    int childrenDisp = -1;
    for (size_t i = 0; i + 5 <= 0x190; ++i) {
        if (p[i] == 0x48 && p[i + 1] == 0x8d && p[i + 2] == 0x05) {
            if (nLea < 4) lea[nLea++] = Rel32Target(p + i, 3, 7);
            i += 6;
        } else if (p[i] == 0x48 && p[i + 1] == 0x8d && p[i + 2] == 0x4f) {
            if (childrenDisp < 0) childrenDisp = p[i + 3];
            i += 3;
        } else if (p[i] == 0xe8) {
            if (nCall < 6) call[nCall++] = Rel32Target(p + i, 1, 5);
            i += 4;
        }
    }

    std::snprintf(theirs, sizeof(theirs), "vt=%p/%p/%p call=%p/%p children+0x%x",
                  lea[0], lea[1], lea[2], call[2], call[3],
                  childrenDisp < 0 ? 0xffffu : static_cast<unsigned>(childrenDisp));

    const bool equal =
        (nLea >= 3) && (nCall >= 4) &&
        (lea[0]  == wantVtDlg)  &&
        (lea[1]  == wantVtInt)  &&
        (lea[2]  == wantVtBase) &&
        (call[2] == wantClear)  &&
        (call[3] == wantSetSz)  &&
        (childrenDisp >= 0) &&
        (static_cast<unsigned>(childrenDisp) == wantChildren);
    RtDiffReport(site, equal, ours, theirs);
}

// 0x1800325f0 — LinkedList::Clear. Four displacements: the `next` link the walk
// follows and the three fields it zeroes. Nothing else in that body can be
// wrong without being wrong loudly.
void VerifyListClearShape(RtDiffSite* site) {
    if (!RtDiffEnabled()) return;
    RtDiffRegister(site);

    using LL = Collection::LinkedList<RtObject::InternalBasePtr>;
    char ours[160], theirs[160];
    std::snprintf(ours, sizeof(ours), "next+0x%x tail+0x%x head+0x%x count+0x%x",
                  0x10u,
                  (unsigned)offsetof(LL, _tail),
                  (unsigned)offsetof(LL, _head),
                  (unsigned)offsetof(LL, _count));

    // RT_RAW_OK: an ORACLE read of the shipped bytes, guarded above.
    const uint8_t* p = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x1800325f0));
    if (!p) {
        std::snprintf(theirs, sizeof(theirs), "shipped body not resolvable");
        RtDiffReport(site, false, ours, theirs);
        return;
    }

    //   48 8b 7e 10     MOV RDI,[RSI+0x10]        the `next` read, BEFORE the free
    //   49 89 46 08     MOV [R14+0x8],RAX         tail  = 0
    //   49 89 06        MOV [R14],RAX             head  = 0
    //   41 89 46 10     MOV dword [R14+0x10],EAX  count = 0
    int next = -1, tail = -1, count = -1;
    bool headSeen = false;
    for (size_t i = 0; i + 4 <= 0x9a; ++i) {
        if (p[i] == 0x48 && p[i + 1] == 0x8b && p[i + 2] == 0x7e && next  < 0) next  = p[i + 3];
        if (p[i] == 0x49 && p[i + 1] == 0x89 && p[i + 2] == 0x46 && tail  < 0) tail  = p[i + 3];
        if (p[i] == 0x49 && p[i + 1] == 0x89 && p[i + 2] == 0x06)              headSeen = true;
        if (p[i] == 0x41 && p[i + 1] == 0x89 && p[i + 2] == 0x46 && count < 0) count = p[i + 3];
    }
    std::snprintf(theirs, sizeof(theirs), "next+0x%x tail+0x%x head%s count+0x%x",
                  next < 0 ? 0xffu : (unsigned)next,
                  tail < 0 ? 0xffu : (unsigned)tail,
                  headSeen ? "+0x0" : " ABSENT",
                  count < 0 ? 0xffu : (unsigned)count);

    const bool equal = (next == 0x10) && headSeen &&
                       (tail  >= 0 && (unsigned)tail  == offsetof(LL, _tail)) &&
                       (count >= 0 && (unsigned)count == offsetof(LL, _count));
    RtDiffReport(site, equal, ours, theirs);
}

// The NEGATIVE CONTROL for both of the above, and it RUNS — a shape checker
// that cannot be seen to fail is worth nothing, and this wave has already paid
// for two controls that came back green because they could not fire
// (WAVE9.md, the Picking negative control, and lane COLL's own). It feeds each
// decoder a body that is deliberately NOT the one it expects and requires a
// MISMATCH; if either comes back equal, the decoder is matching on something
// that is not there and its green verdict above means nothing.
//
// The wrong body is `??1DialogController@UI@@UEAA@XZ` for the dtor decoder (a
// real destructor of the same family, so it exercises the decoder rather than
// bouncing off a null) and `0x18007a010`, LinkedList::Remove, for the list one
// (same file, same container, different displacements).
void VerifyLaneUidControls() {
    if (!RtDiffEnabled()) return;
    static RtDiffSite sNeg = { "WAVE9 lane UID shape decoders [negative control]",
                               0, 0, false, false };
    RtDiffRegister(&sNeg);

    // RT_RAW_OK: oracle reads, guarded above.
    const uint8_t* wrongDtor = static_cast<const uint8_t*>(
        ResolveExportRaw("??1DialogController@UI@@UEAA@XZ"));
    const uint8_t* wrongList = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x18007a010));

    bool dtorFired = true, listFired = true;

    if (wrongDtor) {
        // The DialogController dtor stamps ONE vtable and TAIL-JUMPS (e9, not
        // e8), so a decoder that really requires three LEAs and four CALLs
        // cannot match it.
        int nLea = 0, nCall = 0;
        for (size_t i = 0; i + 5 <= 0x40; ++i) {
            if (wrongDtor[i] == 0x48 && wrongDtor[i + 1] == 0x8d && wrongDtor[i + 2] == 0x05) { ++nLea; i += 6; }
            else if (wrongDtor[i] == 0xe8) { ++nCall; i += 4; }
        }
        dtorFired = !(nLea >= 3 && nCall >= 4);
    }
    if (wrongList) {
        int next = -1, count = -1;
        for (size_t i = 0; i + 4 <= 0x9a; ++i) {
            if (wrongList[i] == 0x48 && wrongList[i + 1] == 0x8b && wrongList[i + 2] == 0x7e && next < 0)
                next = wrongList[i + 3];
            if (wrongList[i] == 0x41 && wrongList[i + 1] == 0x89 && wrongList[i + 2] == 0x46 && count < 0)
                count = wrongList[i + 3];
        }
        // Remove zeroes prev/next and decrements the count through a different
        // encoding entirely, so the full four-way match must NOT hold.
        listFired = !(next == 0x10 && count == 0x10);
    }

    char ours[96], theirs[160];
    std::snprintf(ours, sizeof(ours), "both decoders REJECT a body that is not theirs");
    std::snprintf(theirs, sizeof(theirs), "dtor-decoder rejected=%s list-decoder rejected=%s%s",
                  dtorFired ? "yes" : "NO", listFired ? "yes" : "NO",
                  (wrongDtor && wrongList) ? "" : "  (a control body was not resolvable)");
    RtDiffReport(&sNeg, dtorFired && listFired && wrongDtor && wrongList, ours, theirs);
}

}  // namespace

// ============================================================================
// WAVE 9 LANE Z — three DialogInstance vtable slots the CUTOVER needs
// ============================================================================
// The DLL-absent boot asks for four `DialogInstance` virtuals, all four from
// ONE exe-side site (0x7ff71e05a0dc, four times each): the derived-dialog vtable
// builders in code/Game/Dialog/mod_dialog_vtables.cpp,
// code/Game/Dialog/toolbar_vtable_standalone.cpp and
// code/Game/Player/tool_base_toolui.cpp, which copy the base class's slots into
// each derived table. With no DLL those copies are nulls STAMPED INTO A LIVE
// VTABLE — an EXEC-of-0 on the first virtual call, several frames from anything
// naming `DialogInstance`.
//
// Three of the four land here. `?OnClosed@` is DECLINED and why is recorded at
// the end of this block.
// ============================================================================

// ---- ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  0x180114fa0 ord 3322
// ---- ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z
//      0x180115470 ord 3281 -----------------------------------------------
//
// Both are ONE INSTRUCTION in the shipped DLL:
//
//     180114fa0  RET
//     180115470  RET
//
// (three bytes each per Ghidra's function ranges, because MSVC pads them; the
// body is the single `RET`). They are the base class's do-nothing hooks —
// `Object::Internal`'s `Persist` slot and the dialog's settings hook — and every
// derived dialog that cares overrides them.
//
// AN EMPTY BODY IS STILL A PORT, and it is worth stating why rather than
// treating it as trivia. What the cutover needs is not behaviour, it is a
// NON-NULL ADDRESS to stamp into slot 3 and slot 9 of every derived vtable. A
// null there is indistinguishable at the call site from a correct empty
// override right up until it is called. There is deliberately no oracle: a
// function with no reads, no writes and no return value has nothing to compare,
// and `RT_DIFF_BEGIN` is omitted rather than added for the sake of a `calls=`
// row that could only ever be `divergences=0` over nothing
// (reference_probe_needs_an_effectiveness_control.md — a green count over work
// that never happened is the shape to avoid, not to manufacture).
extern "C" void RtUiDialogInstance_Persist(void* /*self*/, void* /*stream*/) {
}

extern "C" void RtUiDialogInstance_OnSerializeSettings(void* /*self*/,
                                                       void* /*stream*/) {
}

// ---- ?TypeName@DialogInstance@UI@@UEBAPEBDXZ  0x180045400  ord 4379 -------
//
//     180045400  MOV RAX,qword ptr [0x1801faac8]
//     180045407  RET
//
// TWO INSTRUCTIONS, and the whole of the work is knowing what 0x1801faac8 is.
// It is not a string pointer of its own: the symbol at 0x1801faac0 is
// `?_typeInfo@DialogInstance@UI@@0VBaseTypeInfo@Object@@A`, and +0x8 into an
// `Object::BaseTypeInfo` is its `name` field (runtime/types/object/TypeInfo.h:
// +0x00 `typeId`, +0x04 unused word, +0x08 `const char* name`). So retail
// returns `_typeInfo.name`, i.e. the literal "DialogInstance" the BaseTypeInfo
// constructor was handed.
//
// The corroboration is the NEXT slot: `?Type@DialogInstance@UI@@UEBAIXZ`
// (0x1800453e0) is `MOV EAX,dword ptr [0x1801faac0] / RET` — the SAME object,
// read at +0x00 for the type id. Two adjacent vtable slots reading +0x00 and
// +0x08 of one address is what makes this a BaseTypeInfo rather than a guess.
//
// RETURNING A STRING LITERAL WAS REJECTED. `"DialogInstance"` would compare
// equal under every `strcmp` in the tree and would need no data slot at all —
// and it would be a different POINTER from the one `Object::Factory::Startup`
// CRC'd into the type id and from the one every other reader of this
// BaseTypeInfo sees. A port whose only failure mode is pointer identity is a
// port that fails silently and late; reading the real field cannot.
//
// ⚠ **THIS PORT NAMES A MISSING MINT ROW, AND THAT IS THE POINT.**
// `?_typeInfo@DialogInstance@UI@@0VBaseTypeInfo@Object@@A` (0x1801faac0) is NOT
// among the 33 `?_typeInfo@` rows in standalone/rt_data_mint_tables.inc, even
// though `??_7DialogInstance@UI@@6B@` IS minted there in full and TWO of its
// slots — this one and `?Type@` — read nothing else. That file is lane C2's and
// this lane did not touch it. Until the row exists, this body dereferences a
// null and dies IN A FUNCTION THAT NAMES ITSELF, which is strictly better than
// the alternative it replaces (a null stamped into a derived vtable, faulting
// later with no name attached) and is exactly the "do not fix a faithful crash
// with a null check" case: the fix is upstream, in the mint, not a guard here.
extern "C" const char* RtUiDialogInstance_TypeName(const void* /*self*/) {
    return *reinterpret_cast<const char* const*>(
        DialogInstanceTypeInfo() + kBaseTypeInfoName);
}

// ---- ?Type@DialogInstance@UI@@UEBAIXZ  0x1800453e0 ------------------------
// WAVE 10B LANE DLGV. The corroborating slot the note above cites, now ported
// rather than only quoted:
//
//     1800453e0  MOV EAX,dword ptr [0x1801faac0]
//     1800453e6  RET
//
// The SAME `?_typeInfo@DialogInstance@UI@@0VBaseTypeInfo@Object@@A` object
// `TypeName` reads at +0x08, read here at +0x00 for the type id. It goes through
// the same `DialogInstanceTypeInfo()` slot, so there is one resolution site for
// the pair and no address door.
//
// The ⚠ note above is now STALE in its one factual claim and is left standing
// because its REASONING is not: `?_typeInfo@DialogInstance@UI@@` was added to
// standalone/rt_data_mint_tables.inc and lane MINTSEL then PROMOTED it, so this
// symbol resolves to our own minted storage on every run. That storage's typeId
// is no longer the 0xFFFFFFFF ctor sentinel — MintTypeInfos() computes it with
// the same `TypeNameCRC(name)` line `Factory::Startup` uses, and the mint
// shadow compares all 104 against the loaded DLL's own nodes with 0 mismatches.
// So this body reads a value that has been checked against retail's, which is a
// stronger position than the null-deref this comment used to describe, and a
// weaker one than reading the DLL's static directly would be. Stated rather than
// hidden: the id is verified by the shadow's field comparison, not by an oracle
// on this body.
constexpr int kBaseTypeInfoTypeId = 0x00;
extern "C" uint32_t RtUiDialogInstance_Type(const void* /*self*/) {
    return *reinterpret_cast<const uint32_t*>(
        DialogInstanceTypeInfo() + kBaseTypeInfoTypeId);
}

// ---- ?OnClosed@DialogInstance@UI@@UEAAXXZ  0x180115330  ord 3269
//
// PORTED — Wave 9 lane ENC-2, in runtime/src/ui/dialog_settings.cpp alongside
// ?SaveSettings@DialogInstance@. The DECLINED note that stood here was right to
// decline and WRONG about the size of the job, in a way worth recording:
//
//   it said "it becomes a one-function job the moment ?SaveSettings@ lands".
//
// It does not. SaveSettings builds a BinaryWriter and then dispatches into it,
// so it needs FOUR more bodies that were themselves unported minted traps --
// the two scalar Serialize slots (+0x98, +0xa0), the back-patching destructor
// ??1BinaryWriter, and BinaryWriter's unexported deleting destructor at slot 0.
// Those landed first, in runtime/src/io/binary_writer.cpp. The row was six
// functions, not one, and counting it as one is the same underestimate that
// made lane ENC four functions instead of the two it was scoped for.
//
// The controller loop's slot +0x30 that this note flagged as unnamed is now
// named by the EXPORT TABLE rather than by a caller: ??_7DialogController@UI@@6B@
// (0x1801565f0) was read byte-wise and slot +0x30 is
// ?OnClosed@DialogController@UI@@UEAAXXZ (0x1800cd290), with +0x50
// ?OnSerializeSettings@DialogController@UI@@ (0x1800cd250). Both are in
// dialog_settings.cpp's header with the evidence.

// ============================================================================
// UI::DialogInstance — const accessors
// ============================================================================

// ?GetRoot@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ
// Retail 0x180045580, ordinal 2875. Profile build: same one-liner.
//   MOV RAX,[RCX+0x50] / MOV [RDX],RAX / MOV RAX,RDX / RET
void** RtUiDialogInstance_GetRoot(DialogInstance* self, void** out) {
    RT_DIFF_BEGIN("UI::DialogInstance::GetRoot");
    *out = self->_root;
    if (rt_diff_on__) {
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(PtrOutFn, kGetRoot)(self, &theirs);
        DiffPtr(&rt_diff_site__, *out, theirs);
    }
    return out;
}

// ?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z
// Retail 0x180113320, ordinal 2513.
//
// THE PARAMETER IS A TYPE ID, NOT AN INDEX. The loop walks every controller
// and compares the argument against the controller's OWN type id — vtable slot
// +0x10, which runtime/types/object/Internal.h independently names
// kInternalVtblType. engine_funcs.h:496 calls it `index`; that comment is
// corrected as part of this lane (PLAN §6 step 8).
//
// Note the two counters retail keeps: EBX is the element index used for the
// final re-index, RDI is the same thing pre-multiplied by 8. Reproduced with
// one index because the two can never disagree.
void** RtUiDialogInstance_GetController(DialogInstance* self, void** out, uint32_t typeId) {
    RT_DIFF_BEGIN("UI::DialogInstance::GetController");
    void* found = nullptr;
    for (int32_t i = 0; i < self->_controllers._size; ++i) {
        DialogController* c = self->_controllers._data[i];
        // No null check: retail loads [RCX] off the controller unguarded.
        void** vt = *reinterpret_cast<void***>(c);
        const uint32_t id = reinterpret_cast<TypeIdFn>(vt[RtUI::kControllerVtblGetTypeId / 8])(c);
        if (id == typeId) {
            found = self->_controllers._data[i];
            break;
        }
    }
    *out = found;
    if (rt_diff_on__) {
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(PtrOutU32Fn, kGetController)(self, &theirs, typeId);
        DiffPtr(&rt_diff_site__, *out, theirs);
    }
    return out;
}

// ?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z
// Retail 0x180114c70, ordinal 2346. A forwarder:
//   MOV RCX,[RCX+0x50] / CALL Element::FindElement / MOV RAX,RBX / RET
// NO NULL CHECK on _root — retail has none, and CLAUDE.md's rule applies: a
// guard here would not restore the behaviour, it would skip it. If this faults
// the dialog has no root element and THAT is the bug.
void** RtUiDialogInstance_FindElement(DialogInstance* self, void** out, const void* name) {
    RT_DIFF_BEGIN("UI::DialogInstance::FindElement");
    reinterpret_cast<PtrOutStrFn>(ResolveExport(kElementFindElement))(self->_root, out, name);
    if (rt_diff_on__) {
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(PtrOutStrFn, kFindElement)(self, &theirs, name);
        DiffPtr(&rt_diff_site__, *out, theirs);
    }
    return out;
}

// ?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z
// Retail 0x1801140c0, ordinal 2348. The same forwarder against
// Element::FindElementType. Same absent null check, same reason.
void** RtUiDialogInstance_FindElementType(DialogInstance* self, void** out, uint32_t typeId) {
    RT_DIFF_BEGIN("UI::DialogInstance::FindElementType");
    reinterpret_cast<PtrOutU32Fn>(ResolveExport(kElementFindElementType))(self->_root, out, typeId);
    if (rt_diff_on__) {
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(PtrOutU32Fn, kFindElementType)(self, &theirs, typeId);
        DiffPtr(&rt_diff_site__, *out, theirs);
    }
    return out;
}

// ============================================================================
// UI::DialogInstance — self-contained mutators
// ============================================================================

// ?SetFlagsOnOff@DialogInstance@UI@@QEAAXII@Z
// Retail 0x180114690, ordinal 3882. Five instructions:
//   OR EDX,[RCX+0x20] / NOT R8D / AND EDX,R8D / MOV [RCX+0x20],EDX / RET
// so the ON mask is applied FIRST and the OFF mask wins any overlap.
void RtUiDialogInstance_SetFlagsOnOff(DialogInstance* self, uint32_t on, uint32_t off) {
    RT_DIFF_BEGIN("UI::DialogInstance::SetFlagsOnOff");
    DialogInstance* shadow = ReplaySnapshot(rt_diff_on__, self);

    self->_flags = (on | self->_flags) & ~off;

    if (rt_diff_on__) {
        RT_ORIGINAL_ONCE(SetFlagsFn, kSetFlagsOnOff)(shadow, on, off);
        RT_DIFF_BYTES(self, shadow, kDialogInstanceCtorExtent);
    }
}

// ?StopTimer@DialogInstance@UI@@QEAAXH@Z
// Retail 0x180113730, ordinal 4111. Transcribed from the DISASSEMBLY, not the
// decompiled C, because two details do not survive decompilation:
//
//   * `MOVSXD R10,[RCX+0x70]` — the count is sign-extended to 64 bits and the
//     search loop's bound is that 64-bit value, while the SHIFT loop below
//     re-reads the 32-bit field each iteration.
//   * `ADD RCX,RDX / JZ` at 0x180113786 — the "is the entry non-null" test is
//     against `data + index*0x18`, i.e. against the SUM, not against `data`.
//     It can only be taken when the array pointer plus the offset is exactly
//     zero. Reproduced literally; it is retail's own dead-ish guard and
//     removing it would change nothing except our fidelity to the shipped code.
//
// The shift is a copy-down of three qwords (0x18 bytes) per entry, and the
// count is decremented ONCE at the end — not per entry.
void RtUiDialogInstance_StopTimer(DialogInstance* self, int32_t id) {
    RT_DIFF_BEGIN("UI::DialogInstance::StopTimer");

    // ---- oracle setup: snapshot the object AND redirect its timer array ----
    DialogInstance* shadow = nullptr;
    bool diffThisCall = false;
    if (rt_diff_on__) {
        const int32_t cap = self->_timers._capacity;
        const size_t bytes = (cap > 0 ? static_cast<size_t>(cap) : 0u) * sizeof(DialogTimer);
        if (bytes <= kReplayTimerBytes) {
            std::memcpy(g_replayObj, self, sizeof(DialogInstance));
            shadow = reinterpret_cast<DialogInstance*>(g_replayObj);
            if (self->_timers._data && bytes) {
                std::memcpy(g_replayTimers, self->_timers._data, bytes);
                shadow->_timers._data = reinterpret_cast<DialogTimer*>(g_replayTimers);
            }
            diffThisCall = true;
        }
        // else: this call is not diffed at all. See the note on
        // kReplayTimerBytes — reporting a divergence here would be a fact about
        // the oracle's buffer, not about the port.
    }

    // ---- the port ----------------------------------------------------------
    const int64_t count64 = static_cast<int64_t>(self->_timers._size);
    if (count64 > 0) {
        uint8_t* base = reinterpret_cast<uint8_t*>(self->_timers._data);
        int32_t  i = 0;
        int64_t  walked = 0;
        // Search: compare the id at entry+0x10.
        for (;;) {
            if (*reinterpret_cast<const int32_t*>(base + walked * 0x18 + 0x10) == id) break;
            ++walked;
            ++i;
            if (walked >= self->_timers._size) return;   // retail: plain RET
        }
        if (i < self->_timers._size) {
            int64_t off = static_cast<int64_t>(i) * 0x18;
            do {
                ++i;
                uint8_t* p = base + off;
                if (i < self->_timers._size && p != nullptr) {
                    std::memcpy(p + 0x00, p + 0x18, 8);
                    std::memcpy(p + 0x08, p + 0x20, 8);
                    std::memcpy(p + 0x10, p + 0x28, 8);
                }
                off += 0x18;
            } while (i < self->_timers._size);
        }
        self->_timers._size -= 1;
    }

    // ---- oracle comparison -------------------------------------------------
    if (diffThisCall) {
        RT_ORIGINAL_ONCE(StopTimerFn, kStopTimer)(shadow, id);

        // Fingerprint = the ctor-initialised object prefix with the _timers
        // data POINTER blanked (we redirected the shadow's on purpose), then
        // the live timer entries. Comparing the raw pointer would report a
        // divergence the oracle itself introduced.
        auto fingerprint = [](uint8_t* dst, const DialogInstance* obj,
                              const DialogTimer* entries) -> size_t {
            std::memcpy(dst, obj, kDialogInstanceCtorExtent);
            std::memset(dst + offsetof(DialogInstance, _timers._data), 0, sizeof(void*));
            size_t n = kDialogInstanceCtorExtent;
            const int32_t size = obj->_timers._size;
            if (entries && size > 0) {
                size_t bytes = static_cast<size_t>(size) * sizeof(DialogTimer);
                if (bytes > kReplayTimerBytes) bytes = kReplayTimerBytes;
                std::memcpy(dst + n, entries, bytes);
                n += bytes;
            }
            return n;
        };
        const size_t nOurs   = fingerprint(g_fpOurs,   self,   self->_timers._data);
        const size_t nTheirs = fingerprint(g_fpTheirs, shadow, shadow->_timers._data);
        RT_DIFF_BYTES(g_fpOurs, g_fpTheirs, nOurs < nTheirs ? nOurs : nTheirs);
    }
}

// ============================================================================
// UI::DialogInstance — the three controller-dispatch virtuals
// ============================================================================
// All three are `UEAA`, i.e. VIRTUAL, and the DLL's exported
// ??_7DialogInstance@UI@@6B@ is what the DLL itself dispatches through — so
// registering these redirects our direct `fn_DialogInstance_*` callers and
// nothing else. That is intentional (see the safety note at the top).
//
// The loops carry NO null check on the controller pointer: retail's
// `MOV RCX,[RDI+RAX] / MOV RAX,[RCX]` dereferences it unguarded. Kept.

// ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ — retail 0x1801153a0, ord 3289.
void RtUiDialogInstance_OnUpdateLayout(DialogInstance* self) {
    for (int32_t i = 0; i < self->_controllers._size; ++i) {
        DialogController* c = self->_controllers._data[i];
        void** vt = *reinterpret_cast<void***>(c);
        reinterpret_cast<void (*)(void*)>(vt[RtUI::kControllerVtblOnUpdateLayout / 8])(c);
    }
}

// ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z — retail 0x180115400, ord 3286.
void RtUiDialogInstance_OnUpdate(DialogInstance* self, const void* time) {
    for (int32_t i = 0; i < self->_controllers._size; ++i) {
        DialogController* c = self->_controllers._data[i];
        void** vt = *reinterpret_cast<void***>(c);
        reinterpret_cast<void (*)(void*, const void*)>(
            vt[RtUI::kControllerVtblOnUpdate / 8])(c, time);
    }
}

// ?OnElementEvent@DialogInstance@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z
// Retail 0x180114ed0, ordinal 3274.
//
// The first parameter is an `InternalPtr<Element>` BY VALUE, which at this size
// MSVC passes by address — retail receives a pointer in RDX, and re-reads
// `*RDX` into a fresh stack slot on EVERY iteration before handing the ADDRESS
// of that slot to the controller. Reproduced exactly: the controller is handed
// a pointer to our own copy, never the caller's, so a controller that
// overwrites its argument cannot corrupt the caller's InternalPtr. Collapsing
// that into "pass elementPtr through" would be a real behavioural change.
void RtUiDialogInstance_OnElementEvent(DialogInstance* self, void** elementPtr,
                                       int32_t a, int32_t b, int32_t c4,
                                       int32_t d, int32_t e, void* userData) {
    for (int32_t i = 0; i < self->_controllers._size; ++i) {
        DialogController* c = self->_controllers._data[i];
        void* local = *elementPtr;
        void** vt = *reinterpret_cast<void***>(c);
        reinterpret_cast<void (*)(void*, void**, int32_t, int32_t, int32_t,
                                  int32_t, int32_t, void*)>(
            vt[RtUI::kControllerVtblOnElementEvent / 8])(c, &local, a, b, c4, d, e, userData);
    }
}

// ============================================================================
// UI::Dialog
// ============================================================================

// ?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z
// Retail 0x1800388b0, ordinal 2956. Six instructions, no bounds check:
//   MOV RAX,[RCX+0x68] / MOVSXD RCX,R8D / MOV RCX,[RAX+RCX*8] /
//   MOV RAX,RDX / MOV [RDX],RCX / RET
// The MOVSXD is why `index` is int32_t and not uint32_t
// (reference_ghidra_c_output_vs_disassembly.md: Ghidra prints a MOVSXD as a
// zero extension). A negative index reads BEFORE the array, faithfully.
void** RtUiDialog_GetTemplate(Dialog* self, void** out, int32_t index) {
    RT_DIFF_BEGIN("UI::Dialog::GetTemplate");
    *out = self->_templates[static_cast<int64_t>(index)];
    if (rt_diff_on__) {
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(PtrOutI32Fn, kGetTemplate)(self, &theirs, index);
        DiffPtr(&rt_diff_site__, *out, theirs);
    }
    return out;
}

// ============================================================================
// UI::DialogController
// ============================================================================

// ?GetContainer@DialogController@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ
// Retail 0x18003b700, ordinal 2508. The same one-liner as GetRoot, at +0x20.
void** RtUiDialogController_GetContainer(DialogController* self, void** out) {
    RT_DIFF_BEGIN("UI::DialogController::GetContainer");
    *out = self->_container;
    if (rt_diff_on__) {
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(PtrOutFn, kGetContainer)(self, &theirs);
        DiffPtr(&rt_diff_site__, *out, theirs);
    }
    return out;
}

// ??1DialogController@UI@@UEAA@XZ — retail 0x1800cd2f0, ordinal 572.
//
// The whole body is: stamp ??_7DialogController@UI@@6B@ over the vtable slot,
// then TAIL-JUMP to Object::Internal::~Internal (0x1800bfdb0), which is the
// handle-table swap-remove. We do not reimplement that swap-remove here — we
// call the same exported dtor retail's JMP lands in, through the SEAM
// (ResolveExport), so if Wave 3's ~Internal port is registered we follow it
// exactly as a cutover build would.
//
// The vtable is likewise the DLL's own exported table, resolved by name. This
// port mints nothing (PLAN §4.3 risk 3).
void RtUiDialogController_Dtor(DialogController* self) {
    static void* const vtbl = ResolveExport("??_7DialogController@UI@@6B@");
    static void* const internalDtor = ResolveExport("??1Internal@Object@@UEAA@XZ");
    *reinterpret_cast<void**>(self) = vtbl;
    reinterpret_cast<void (*)(void*)>(internalDtor)(self);
}


// ============================================================================
// WAVE 9 LANE UID — ??1DialogInstance@UI@@UEAA@XZ  0x180114d40  ord 574
// ============================================================================
//
// The dialog lifecycle's teardown half, declined by lane UIF (row 6, "the one
// genuinely close call") and again by lane UIC (§7). BOTH DECLINES RESTED ON
// THREE STATED BLOCKERS AND ALL THREE DISSOLVED ON RE-CHECKING
// (reference_holdback_blockers_are_stale_check_them_first.md). Each is answered
// from the bytes below rather than argued:
//
//  1. "0x18002fca0 (int)" — NOT unported. `ArrayPtrSetSize` in
//     runtime/src/object/internal.cpp has been the port of FUN_18002fca0 since
//     Wave 3, with the address in the comment directly above it. What was
//     missing was the REGISTRATION at that address, which this lane added
//     (internal.cpp, ungated). A half-registered port reads exactly like an
//     absent one from a callee census, which is how it survived two lanes.
//  2. "0x1800325f0 (int, list free-all)" — now ported, as
//     `RtColl_LinkedListClear_1800325f0` in runtime/src/collection/linkedlist.cpp,
//     the fourth member of the `Collection::LinkedList<T=8>` family lane UIC
//     opened. ~20 lines; its whole failure surface is four displacements and
//     they are shape-checked below.
//  3. "the body OPEN-CODES the InternalFactory swap-remove inline at
//     0x180114e71 … in a function with no oracle, and it is the one row where
//     being wrong CORRUPTS THE HANDLE TABLE."
//
//     This one is the substance of both declines, and it is answered by reading
//     `??1Internal@Object@@UEAA@XZ` (0x1800bfdb0) beside it. THE TWO ARE THE
//     SAME INSTRUCTION STREAM, register allocation aside:
//
//       0x1800bfdb0  ~Internal              0x180114e64  ~DialogInstance tail
//       ---------------------------------  ---------------------------------
//       LEA RAX,[0x180154720]              LEA RAX,[0x180154720]
//       MOV [RCX],RAX                      MOV [RDI],RAX
//       MOV dword [RCX+0xc],0x0            MOV dword [RDI+0xc],EBP   (EBP == 0)
//       MOV RCX,[0x1801fc6d8]              MOV RCX,[0x1801fc6d8]
//       MOV EDX,[RCX+0x8] / DEC EDX        MOV EDX,[RCX+0x8] / DEC EDX
//       MOVSXD R11,[RBX+0x8]               MOVSXD R11,[RDI+0x8]
//       CMP EDX,R11D / JZ skip             CMP EDX,R11D / JZ skip
//       MOV R9,[RCX] / LEA R10,[R11*8]     MOV R9,[RCX] / LEA R10,[R11*8]
//       MOVSXD RAX,EDX                     MOVSXD RAX,EDX
//       MOV R8,[R9+RAX*8]                  MOV R8,[R9+RAX*8]
//       MOV [R9+R10],R8                    MOV [R9+R10],R8
//       MOV RAX,[RCX] / MOV R8,[RAX+R10]   MOV RAX,[RCX] / MOV R8,[RAX+R10]
//       MOV [R8+0x8],R11D                  MOV [R8+0x8],R11D
//       skip: CALL 0x18002fca0             skip: CALL 0x18002fca0
//       LEA RAX,[0x180154788]              LEA RAX,[0x180154788]
//       MOV [RBX],RAX                      MOV [RDI],RAX
//
//     So retail's "open-coded swap-remove" IS `Object::Internal::~Internal`,
//     INLINED VERBATIM — not a second, private handle-table mutator. This port
//     therefore does what `??1DialogController@UI@@` (60 lines above, Wave 7,
//     shape-verified) already does for the identical tail: it calls the ONE
//     body the tree owns, through the seam, so exactly one handle-table mutator
//     is ever in play and it is whichever one the `BANISHED_RT_OBJECT_TABLE`
//     tier has armed. Nothing here reimplements the swap-remove, and nothing
//     here arms a tier that is off.
//
//     ⚠ THE COUPLING IS REAL AND IS NOT HIDDEN: with the DLL present and that
//     tier off (the default), this dtor's tail is the DLL's `~Internal`, which
//     is correct. AT CUTOVER the tier must be armed — WAVE9.md already records
//     it as "Opt-in, and MANDATORY at cutover", so this adds no new obligation,
//     but it does add a caller that will fault loudly if the promise is broken.
//
// WHAT THE BODY IS, in retail's own order (0x180114d40-0x180114ec9):
//
//     this->vptr = ??_7DialogInstance@UI@@6B@          (0x180156d70)
//     if (_root) { _root->vtbl[0](_root, 1); _root = 0; }        // scalar-del
//     for (i = 0; i < _controllers._size; ++i) {
//         c = _controllers._data[i];                  // RE-READ each iteration
//         if (c) { c->vtbl[0](c, 1); _controllers._data[i] = 0; }
//     }
//     if (_controllers._data) MemFree(_controllers._data);
//     _controllers._data = 0;  _controllers._size = 0;   // NOT _capacity
//     if (_timers._data)      MemFree(_timers._data);
//     _timers._data = 0;       _timers._size = 0;        // NOT _capacity
//     LinkedList::Clear(&_children);                   // 0x1800325f0
//     Object::Internal::~Internal(this);               // inlined, see above
//
// FOUR DETAILS THAT A TIDIER REWRITE WOULD GET WRONG:
//   * `_controllers._data` is RE-LOADED INSIDE the loop (0x180114da0), not
//     hoisted. A controller's own destructor can reallocate the array it is
//     being destroyed out of, and retail re-reads for that reason or by
//     accident — either way, hoisting changes what the next iteration sees.
//   * `_capacity` IS LEFT ALONE for both arrays. Only data and size are zeroed.
//   * THE `if (ptr)` BEFORE EACH MemFree IS RETAIL'S OWN redundancy — MemFree
//     (0x1800c7ad0) opens with `TEST RCX,RCX; JZ ret`. It is reproduced rather
//     than dropped; this is not a guard added to paper over a crash.
//   * SLOT 0 IS CALLED WITH FLAG 1 — the SCALAR DELETING destructor, which
//     frees the child. Passing 0 would destruct without freeing and leak every
//     element and controller of every dialog.
//
// ORACLE: NONE, and none is possible — it is a destructor, so a replay is not a
// comparison but a second destruction: it would run every child's deleting
// destructor again, free both arrays twice, and swap-remove a handle that no
// longer exists. It ships in the disposition lanes M, Z and UIF established: a
// no-oracle port with NO call into the shipped body at all.
//
// What stands in for it is a SHAPE CHECK, `VerifyDialogInstanceDtorShape`,
// which reads the shipped instruction stream and compares the three vtable
// addresses this body stamps, the two functions it calls, and the `_children`
// displacement it hands to the first of them. That is not "some" of the failure
// surface — for a destructor whose every other operation is a store of zero,
// those six values ARE what could silently be wrong, and all six are compared
// against the DLL while the DLL is still there (WAVE9.md §2: the verification
// budget has to be spent before the removal).
void RtUiDialogInstance_Dtor(DialogInstance* self) {
    // LIVENESS ONLY, not a comparison. `calls=` will read 0 here because
    // nothing is diffed; `worked=` is the whole signal and it answers the one
    // question a no-oracle port cannot otherwise answer -- did this body EVER
    // RUN (reference_uncalled_port_is_not_verified.md). It counts calls that
    // had real teardown to do, so a dtor reached only on an already-empty
    // dialog cannot inflate it.
    //
    // THREAD SAFETY, stated rather than assumed: rt_diff.cpp has NO
    // synchronisation (WAVE9.md §2), so a site here would be a data race if
    // dialogs were torn down off the main thread. They are not -- every
    // `~DialogInstance` reachable from this tree runs from
    // `UI::UserInterface::Update` / `CleanRemovedDialogs` / `CloseDialog`, all
    // of which are called from `Engine::MainLoop::Frame` on the main thread.
    RT_DIFF_BEGIN("UI::DialogInstance::~DialogInstance [liveness]");
    RT_DIFF_WORK(self->_root != nullptr || self->_controllers._size > 0 ||
                 self->_children._head != nullptr);

    uint8_t* p = reinterpret_cast<uint8_t*>(self);

    *reinterpret_cast<void**>(p) = VtblDialogInstance();

    // --- the root Element, through its scalar deleting destructor ------------
    if (self->_root) {
        DeletingDtorOf(self->_root)(self->_root, 1);
        self->_root = nullptr;
    }

    // --- every controller, same way. _data is re-read each iteration ---------
    for (int i = 0; i < self->_controllers._size; ++i) {
        DialogController** data = self->_controllers._data;   // re-read: see above
        DialogController*  c    = data[i];
        if (c) {
            DeletingDtorOf(c)(c, 1);
            data[i] = nullptr;
        }
    }

    if (self->_controllers._data) MemFreeStatic(self->_controllers._data);
    self->_controllers._data = nullptr;
    self->_controllers._size = 0;      // _capacity deliberately untouched

    if (self->_timers._data) MemFreeStatic(self->_timers._data);
    self->_timers._data = nullptr;
    self->_timers._size = 0;           // _capacity deliberately untouched

    ListClear(&self->_children);

    // Retail inlines Object::Internal::~Internal here; the instruction-for-
    // instruction comparison is in the block above. Reached through the seam so
    // that exactly one handle-table mutator is in play.
    InternalDtor(self);
}

// ============================================================================
// UI::ElementDescription
// ============================================================================

// ??0ElementDescription@UI@@QEAA@XZ — retail 0x180115a40, ordinal 173.
//
// Three vtable stores in base-class order, then the field initialisers. The
// two non-zero seeds are the interesting part and both are reproduced exactly:
//   +0x3c = -1        (the element id; -1 means "unset", 0 is a REAL id —
//                      reference_unresolved_lookup_leaves_a_valid_looking_zero.md)
//   +0x40 = 3         (as a QWORD, so +0x44, the flags, is zeroed with it)
//
// Every byte of 0x00..0x67 is written, which is what makes the replay below a
// full-object oracle rather than a spot check.
void* RtUiElementDescription_Ctor(ElementDescription* self) {
    RT_DIFF_BEGIN("UI::ElementDescription::ctor");

    static void* const vtblBase = ResolveExport(RtUI::kVtblBase);
    static void* const vtblExt  = ResolveExport(RtUI::kVtblExternal);
    static void* const vtblEd   = ResolveExport(RtUI::kVtblElementDescription);

    alignas(16) static uint8_t scratch[sizeof(ElementDescription)];
    if (rt_diff_on__) std::memcpy(scratch, self, sizeof(ElementDescription));

    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    *reinterpret_cast<void**>(p + 0x00) = vtblBase;   // Object::Base
    *reinterpret_cast<void**>(p + 0x00) = vtblExt;    // Object::External
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;       // External::_path (Filename)
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;       // External::_refs data
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;       // External::_refs size/capacity
    *reinterpret_cast<void**>(p + 0x00) = vtblEd;     // UI::ElementDescription
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;       // _x, _y
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;       // _width, _height
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;       // _minWidth, _minHeight
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;       // _tabOrder
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;      // _id
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;       // _elementType = 3, _flags = 0
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;       // _childDesc
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;       // _userData
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;       // _stringTable
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;       // _textName

    if (rt_diff_on__) {
        RT_ORIGINAL_ONCE(CtorFn, kEdCtor)(scratch);
        RT_DIFF_BYTES(self, scratch, sizeof(ElementDescription));

        // WAVE 10 LANE UIF: `?CreateInstance@ElementDescription` has no live
        // caller on any route this harness can drive (see its twin's banner),
        // so its oracle is triggered from here — the earliest live call site in
        // the class that is guaranteed to be past allocator startup. The flag
        // is set BEFORE the call because the twin constructs descriptions of
        // its own and would otherwise re-enter.
        static bool edTwinDone = false;
        if (!edTwinDone) { edTwinDone = true; VerifyElementDescriptionCreateInstanceTwin(); }
    }
    return self;   // MOV RAX,RCX — a ctor returns `this`
}

// ?GetFileVersion@ElementDescription@UI@@UEBAHXZ — retail 0x180116af0, ord 2624.
//   MOV EAX,0x4 / RET
// The constant is the .rsc element-description file version and it is what
// ?Serialize@ (held back) is written against.
int32_t RtUiElementDescription_GetFileVersion(const ElementDescription* self) {
    RT_DIFF_BEGIN("UI::ElementDescription::GetFileVersion");
    const int32_t ours = 4;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(IntFn, kEdFileVersion)(self));
    return ours;
}

// ============================================================================
// ??1ElementDescription@UI@@UEAA@XZ  retail 0x180116100  ord 585
//                                                          (WAVE 9 LANE UIF)
// ============================================================================
//
// WHY THIS ROW EXISTS AT ALL. Like `??1Element`, this destructor is not reached
// by any direct C++ caller in our tree — it is reached through a LIVE MINTED
// VTABLE. `??_7ElementDescription@UI@@6B@` slot 0 is the vector deleting
// destructor `??_EElementDescription@UI@@` at 0x180035350, one of the 37 Wave 9
// lane K ported, and lane K reaches `~Class` by ADDRESS through ResolveAddr.
// With this row unported that edge went to the DLL, and after cutover to NULL.
//
// 0x180035350 WAS DISASSEMBLED FOR THIS LANE, and it is one of lane K's "22 of
// 37 that INLINE the scalar branch": only its ARRAY branch names the destructor
// (`LEA R9,[0x180116100]` at 0x180035377), while its scalar branch open-codes
// the same instructions. That is exactly why lane K's port calls `~Class` on
// BOTH branches rather than reproducing the inlining — so this registration is
// what both branches now land on.
//
// THE BODY, from the disassembly at 0x180116100 (13 instructions, one tail
// jump), and it is small enough to quote in full:
//
//     this->_vtable = &??_7ElementDescription@UI@@6B@;   // 0x1801561c0
//     <release this->_textName>                          // +0x60, the ONE String
//     JMP ~External(this);                               // 0x1800bc950, TAIL
//
// runtime/types/ui/ElementDescription.h already says this in its SIZE argument
// ("`??1ElementDescription` (0x180116100) releases exactly one String, the one
// at +0x60, before chaining"), written by Wave 7 lane B from the same bytes.
// The port agreeing with a header derived independently is a weak check, but it
// is a real one and it is free.
//
// EVERY CALLEE IS SATISFIED — the admission test, and this is the cheapest row
// in the cluster to pass it:
//   * 0x1800c7ad0 MemFree      — PORTED, lane Q
//   * 0x1800bc950 ~External    — PORTED, Wave 9 lane B2 (?? 1External@Object@@)
// Two callees, both ours. The row CLOSES; it does not move one frame deeper.
//
// NO ORACLE, for the same reason as `??1Element` and stated for the same
// reason: a replay would decrement the `_textName` refcount a second time and,
// where our body already took it to zero, free a block twice and then run
// `~External` twice over a handle that no longer exists. `RT_DIFF_WORK` records
// how many calls actually released a buffer; `calls=` stays 0 because there is
// no comparison to report. Liveness, not correctness.
void RtUiElementDescription_Dtor(ElementDescription* self) {
    RT_DIFF_BEGIN("UI::ElementDescription::~ElementDescription");
    bool didWork = false;

    static void* const vtblEd = ResolveExport(RtUI::kVtblElementDescription);
    if (vtblEd) *reinterpret_cast<void**>(self) = vtblEd;

    // Retail's String release, with its asymmetry intact: the FREEING branch
    // leaves the field holding `data - 8` (dangling), the SHARING branch nulls
    // it. See runtime/types/system/String.h.
    if (char* p = self->_textName) {
        if (--RtSystem::StringRefCount(p) == 0) {
            self->_textName = p - 8;
            using FreeFn = void (*)(void*);
            static FreeFn freeFn = reinterpret_cast<FreeFn>(
                ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
            if (freeFn) freeFn(self->_textName);
            didWork = true;
        } else {
            self->_textName = nullptr;
        }
    }

    RT_DIFF_WORK(didWork);

    // ~External — a TAIL JMP in retail, a normal call here (the same single
    // deliberate departure `??1Element` makes, for the same reason).
    //
    // Reached by NAME rather than by address: 0x1800bc950 IS exported, as
    // `??1External@Object@@UEAA@XZ` (checked against the shipped export
    // directory with pefile before this line was written — a name the DLL does
    // not contain resolves NULL in silence and the port still registers,
    // reference_resolveexport_phantom_names.md). The name door is the one this
    // file already uses for every ported sibling, and `UEAA` is the virtual
    // spelling, which is what this destructor is; `QEAA` would resolve null
    // behind the guard below (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
    {
        using Fn = void (*)(void*);
        static Fn fn = reinterpret_cast<Fn>(
            ResolveExport("??1External@Object@@UEAA@XZ"));
        if (fn) fn(self);
    }
}

// ============================================================================
// WAVE 9 LANE UIC — the two rows WAVE9_LANE_UIF.md declined at 8 and 11
// ============================================================================

// ---- ?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@
//      V?$ExternalPtr@VElementDescription@UI@@@4@@Z   0x180112ed0  ord 1888 ---
//
// "Make an element from this description, unless I have a NAMED TEMPLATE that
// overrides it." The named-template table at Dialog+0x58/+0x60 is already
// recovered — runtime/types/ui/Dialog.h's `DialogNamedTemplate`, stride 0x10,
// {ExternalPtr +0x00, String +0x08} — and this body is what pinned it.
//
// ABI. `QEBA` returning a class: RCX = this, RDX = the HIDDEN RETURN SLOT,
// R8 = the `ExternalPtr<ElementDescription>` argument, and RAX = RDX at the RET
// (`MOV RAX,RSI` at 0x180113089). engine/funcs/engine_funcs.h:689 already
// declares the three-argument shape, and counting arguments against the mangled
// name agrees: one by-value pointer-sized argument plus the return slot
// (reference_hidden_retslot_wild_write.md — an fn_* typedef that omits the slot
// is a wild write that still returns the right answer).
//
// R8 IS THE DESCRIPTION ITSELF, not a pointer to an ExternalPtr. It is handed
// straight to `?GetPath@External@Object@@` as `this` at 0x180112f08, and in the
// no-match path it is re-loaded from its home slot and its VTABLE is read
// (`MOV RAX,[RCX]` at 0x180113054). An 8-byte ExternalPtr has neither.
//
// THE VIRTUAL CALL AT 0x18011305d IS SLOT 8, AND SLOT 8 IS PURE.
// WAVE9_LANE_UIF.md declined this row on that call being "a virtual dispatch on
// an ElementDescription-family vtable whose slot 8 I did not identify". It is
// identified, out of a table rather than by guessing:
// standalone/rt_data_mint_tables.inc's `??_7ElementDescription@UI@@6B@`
// (kVSlots20, 9 slots) has index 8 — byte offset 0x40 — as `RT_MINT_PURECALL`
// at 0x18007ec28. So it is PURE VIRTUAL on the base and is only ever reached on
// a CONCRETE derived description's vtable. That matters for exactly one reason:
// **porting this body introduces no new cutover symbol.** Retail's own body
// dispatches through the same derived slot; whether that slot is satisfied is
// the description class's problem either way, unchanged by this port.
// Its shape from the call site is
// `virtual InternalPtr<Element> <slot8>(const Dialog*) const`.
//
// THE INLINE STRCMP IS TRANSCRIBED, NOT SUMMARISED. It is guarded by a length
// compare on `String`'s length dword at [p-8] and only then walks bytes, with
// the result formed as `(signed char)ours[i] - (signed char)theirs[i]`.
// reference_uniform_population_is_not_a_verification.md is about precisely this
// construct: a ported inline strcmp that returned 0 for every distinct pair
// sharing a first byte survived four waves of oracles because the population it
// was tested on had thirteen copies of one value. The loop below is written out
// instruction for instruction for that reason, and §"Oracle" in
// docs/runtime_port/WAVE9_LANE_UIC.md records what the population actually was.
//
// The five direct callees are all ours: `GetPath` (0x180006250) and
// `GetPathIdentifer` (0x1800c6610) are ported and registered under both doors
// (runtime/src/object/external.cpp:739/763, runtime/src/system/filename.cpp:
// 450/477), and `MemFree` (0x1800c7ad0, three call sites) is lane E's.
//
// ORACLE: replay, and it is sound here. The body is a PURE READ of the Dialog
// and of the description — the only writes are to the caller's return slot and
// to two String refcounts it also releases before returning, so it is net
// refcount-neutral. Running it twice and comparing the two returned pointers
// therefore compares the whole decision: which table entry matched, and which
// object the virtual was dispatched on. What it does NOT isolate is the virtual
// itself: if slot 8 has a side effect, the replay runs it twice. That is stated
// rather than hidden, and it is the same disposition every replayed Wave 7
// dialog getter carries.
void** RtUiDialog_CreateElement(const Dialog* self, void** ret, void* desc) {
    RT_DIFF_BEGIN("UI::Dialog::CreateElement");

    if (!desc) { *ret = nullptr; RT_DIFF_WORK(false); return ret; }

    // `String name = desc->GetPath().GetPathIdentifer();`
    // Both temporaries are released before the scan, with a STRONG REFERENCE
    // taken on the identifier's buffer first (0x180112f3e `INC [RCX-4]`) —
    // which is why the scan can still read it afterwards.
    using GetPathFn  = void* (*)(const void* self, void* out);
    using GetIdentFn = void* (*)(void* self, void* out);
    static GetPathFn  getPath  = reinterpret_cast<GetPathFn>(
        ResolveExport("?GetPath@External@Object@@QEBA?AVFilename@System@@XZ"));
    static GetIdentFn getIdent = reinterpret_cast<GetIdentFn>(
        ResolveExport("?GetPathIdentifer@Filename@System@@QEBA?AV12@XZ"));

    void* pathTmp  = nullptr;   // System::Filename — first qword is its String
    char* identTmp = nullptr;   // System::String
    char* name     = nullptr;

    if (getPath && getIdent) {
        void* fn = getPath(desc, &pathTmp);
        void* id = getIdent(fn, &identTmp);
        name = *static_cast<char**>(id);
        if (name) { ++RtSystem::StringRefCount(name); }

        // Release identTmp, then pathTmp — retail's order, with retail's
        // asymmetry (the freeing branch leaves the local holding `data - 8`,
        // the sharing branch nulls it). See runtime/types/system/String.h.
        ReleaseStringLocal(identTmp);
        // System::Filename's first qword IS its String buffer — retail releases
        // the Filename temporary by exactly this inline String release on
        // [RSP+0x28] (0x180112f6e..0x180112f9b), never through a ~Filename.
        ReleaseStringLocal(*reinterpret_cast<char**>(&pathTmp));
    }

    // Linear scan of Dialog::_namedTemplates for an entry whose name equals
    // `name`. `_data` and the cursor are both re-read per iteration exactly as
    // retail does (0x180112fb2 re-tests RBX, 0x180112fbe re-loads [R11]).
    void* target  = desc;          // the no-match answer: the argument itself
    bool  matched = false;
    const int32_t count = self->_namedTemplateCount;

    // ---- POPULATION INSTRUMENT, not decoration ----------------------------
    // The inline strcmp below is the construct
    // reference_uniform_population_is_not_a_verification.md is about, and a
    // clean replay proves nothing about it unless the strcmp ACTUALLY RAN on
    // DISTINCT values. It usually does not: the length gate short-circuits
    // every entry whose name is a different length, so a table of twenty names
    // can exercise the byte loop zero times.
    //
    // So the diff message below reports what the population WAS, not just
    // whether the answer matched: how many entries were scanned, how many
    // reached the byte loop at all (a length tie), and how many distinct FIRST
    // BYTES those tied entries had — the first byte being exactly what the
    // known defect keyed on. A reader who sees `tied=0` has learned that this
    // call could not have falsified anything.
    int32_t nScanned = 0, nTied = 0, nTiedEq = 0;
    uint32_t firstByteSeen[8] = {0,0,0,0,0,0,0,0};   // 256-bit set
    auto noteFirstByte = [&](unsigned char c) {
        firstByteSeen[c >> 5] |= (1u << (c & 31));
    };
    for (int32_t i = 0; i < count; ++i) {
        const DialogNamedTemplate& e = self->_namedTemplates[i];

        // Length gate first: String's length dword lives at [p-8], and a null
        // buffer counts as length 0 (0x180112fbc / 0x180112fcc both fall
        // through to EDI, which is 0 here).
        const int32_t lenOurs   = name    ? *reinterpret_cast<const int32_t*>(name    - 8) : 0;
        const int32_t lenTheirs = e._name ? *reinterpret_cast<const int32_t*>(e._name - 8) : 0;
        ++nScanned;
        if (e._name) noteFirstByte(static_cast<unsigned char>(e._name[0]));
        if (lenOurs != lenTheirs) continue;

        if (!name)      { matched = true; }                 // 0x180112fd5
        else if (lenOurs == 0) { matched = true; }          // 0x180112fdb
        else if (!e._name) { matched = false; }             // LEA ECX,[R8+1] -> 1
        else {
            // The inlined strcmp at 0x180112fe8..0x180113021, transcribed.
            const char* a = name;      // "ours"
            const char* b = e._name;   // the table entry
            size_t k = 0;
            unsigned char c = static_cast<unsigned char>(a[0]);
            if (c == static_cast<unsigned char>(b[0])) {
                while (c != 0 && b[k] != 0) {
                    ++k;
                    c = static_cast<unsigned char>(a[k]);
                    if (c != static_cast<unsigned char>(b[k])) break;
                }
            }
            const int diff = static_cast<int>(static_cast<signed char>(a[k])) -
                             static_cast<int>(static_cast<signed char>(b[k]));
            matched = (diff == 0);
            ++nTied;                     // the byte loop actually ran
            if (matched) ++nTiedEq;
        }

        if (matched) { target = e._description; break; }
    }

    if (matched && !target) {
        // 0x180113045: the table entry matched but holds a null description.
        // Retail writes 0 into the return slot and does NOT dispatch.
        *ret = nullptr;
    } else {
        void** vtbl = *reinterpret_cast<void***>(target);
        reinterpret_cast<void* (*)(void*, void**, const Dialog*)>(vtbl[8])(
            target, ret, self);
    }

    // Release the strong reference taken above (0x180113061..0x180113087).
    if (name) ReleaseStringLocal(name);

    if (rt_diff_on__) {
        void*  theirsSlot = nullptr;
        using Fn = void** (*)(const Dialog*, void**, void*);
        RT_ORIGINAL_ONCE(Fn, kCreateElement)(self, &theirsSlot, desc);

        int distinctFirstBytes = 0;
        for (uint32_t w : firstByteSeen)
            for (int b = 0; b < 32; ++b) if (w & (1u << b)) ++distinctFirstBytes;

        // PRINTED UNCONDITIONALLY, not folded into the shadow message.
        // RT_DIFF_SHADOW only emits its strings on a MISMATCH, so a population
        // description carried inside it is invisible on exactly the runs where
        // someone would want to know whether the population could have failed.
        // This is the same instrument bug the AdjustUpperLeft sweep hit from
        // the other side (fourteen comparisons reported as `calls=1`): if the
        // control is only visible when the test fails, it is not a control.
        //
        // Capped so a dialog-heavy run cannot bury the log.
        static int printed = 0;
        if (printed < 24) {
            ++printed;
            std::fprintf(stderr,
                "[UIC-pop] CreateElement #%d: entries=%d scanned=%d "
                "reached-the-byte-loop=%d (equal=%d) distinct-first-bytes=%d "
                "ourName=%s\n",
                printed, count, nScanned, nTied, nTiedEq, distinctFirstBytes,
                name ? name : "(null)");
        }

        char ours[224], their[224];
        std::snprintf(ours, sizeof(ours),
                      "ret=%p  [population] entries=%d scanned=%d "
                      "reached-the-byte-loop=%d (equal=%d) distinct-first-bytes=%d",
                      *ret, count, nScanned, nTied, nTiedEq, distinctFirstBytes);
        std::snprintf(their, sizeof(their), "ret=%p", theirsSlot);
        RT_DIFF_SHADOW(*ret == theirsSlot, ours, their);
    }

    // `worked` is NOT `count > 0`: a scan that never reached the byte loop has
    // not exercised the thing this port can get wrong.
    RT_DIFF_WORK(nTied > 0);
    return ret;
}

// ---- ?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z  0x180114a60  ord 3997 ----
//
// Move a dialog so its top-left sits at (x, y), CLAMPED so the dialog stays on
// screen, and translate the whole element tree by the delta.
//
// `_N` is the third argument: when true, (x, y) arrive in UNSCALED units and
// are multiplied by the UI scale, exactly as the screen size is
// (0x180114abc..0x180114ada). engine/funcs/engine_funcs.h:2453 already spells
// the four-argument signature.
//
// THE UI SCALE IS `_ui->[+0xdc]`, reached as `DialogInstance+0x10` then +0xdc.
// runtime/types/ui/Element.h documents the same float from the other side
// ("?GetPaddedSize@ reaches the UI scale through the ROOT element's copy of
// this: root->_dialogParent[+0x10][+0xdc]"), which is independent
// corroboration of the offset rather than the evidence for it.
//
// `Video::Device::_singleton` (0x1801fe7a8) supplies the screen size at +4/+8.
// It is a MINTED data slot (standalone/rt_data_mint_tables.inc:1565), so it is
// satisfied at cutover; reached here through the same RT_DATA_SLOT door
// runtime/src/ui/userinterface.cpp uses, not by a raw address read.
//
// THE VIRTUAL AT 0x180114ae8 IS ELEMENT SLOT 4 = ComputeSize, which
// runtime/types/ui/Element.h names and standalone/rt_data_mint_tables.inc's
// `??_7Element@UI@@6B@` (kVSlots19) confirms is RT_MINT_PURECALL on the base —
// so, as with CreateElement's slot 8, it is a pure virtual dispatched on the
// concrete element and introduces NO new cutover symbol.
//
// FLAG BITS 0x20 / 0x10 at Element+0x34 mean "this dialog is as wide / as tall
// as the screen". Retail reaches the scale for that branch through the LAST
// element on the parent chain (`while (e->_parent) e = e->_parent;` at
// 0x180114b24..0x180114b36) and then that root's `_dialogParent`, NOT through
// `self->_ui` — a different route to what is normally the same float.
// Reproduced as written; collapsing the two would be an assumption.
//
// THE CLAMP ORDER IS LOAD-BEARING and is transcribed rather than rewritten:
// x is floored at 0 FIRST (CMOVS at 0x180114b8a) and the overflow test
// `w + x > screenW` then uses the ALREADY-FLOORED x (LEA at 0x180114b90). A
// rewrite that clamps both ends symmetrically gives a different answer whenever
// a dialog is wider than the screen.
//
// The trailing translate open-codes AdjustUpperLeft's first border iteration
// inline on the root and then calls it for the border chain. Reproduced in that
// shape.
//
// ORACLE: NONE, and none is manufactured. Replay is not a comparison here but a
// SECOND MOVE: the body's own guard is `delta != 0`, so a second call with the
// same (x, y) finds delta == 0 and does nothing at all — a replay would compare
// "the dialog is where we put it" against itself and report green for a port
// that put it in the wrong place. (That is the same shape as `Audio::Scene::
// Pause`'s early return, recorded in WAVE9.md's holdback table.) What DOES
// carry verification is the enabling port underneath it: every translate this
// body performs goes through `?AdjustUpperLeft@Element@UI@@`, whose
// invertibility oracle in runtime/src/ui/element.cpp checks the whole subtree.
// RT_DIFF_WORK here reports whether the delta was non-zero, so `worked=0` on
// every call means the population never moved a dialog.
void RtUiDialogInstance_SetUpperLeft(DialogInstance* self, int32_t x, int32_t y,
                                     bool scaled) {
    RT_DIFF_BEGIN("UI::DialogInstance::SetUpperLeft");

    const float scale =
        *reinterpret_cast<const float*>(
            static_cast<const uint8_t*>(self->_ui) + 0xdc);

    const uint8_t* device = static_cast<const uint8_t*>(*VideoDeviceSingleton());
    const int32_t screenW = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<const int32_t*>(device + 4)) * scale);
    const int32_t screenH = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<const int32_t*>(device + 8)) * scale);

    if (scaled) {
        x = static_cast<int32_t>(static_cast<float>(x) * scale);
        y = static_cast<int32_t>(static_cast<float>(y) * scale);
    }

    uint8_t* root = static_cast<uint8_t*>(self->_root);

    // Element vtable slot 4 — ComputeSize. Pure virtual on the base; see above.
    {
        void** vtbl = *reinterpret_cast<void***>(root);
        reinterpret_cast<void (*)(void*)>(vtbl[4])(root);
    }

    // The padded size, open-coded: _layout{Width,Height} + the two pads, each
    // floored at _min{Width,Height}.
    int32_t w = *reinterpret_cast<const int32_t*>(root + 0x60) +
                *reinterpret_cast<const int32_t*>(root + 0x1c) +
                *reinterpret_cast<const int32_t*>(root + 0x18);
    int32_t h = *reinterpret_cast<const int32_t*>(root + 0x64) +
                *reinterpret_cast<const int32_t*>(root + 0x24) +
                *reinterpret_cast<const int32_t*>(root + 0x20);
    const uint32_t flags = *reinterpret_cast<const uint32_t*>(root + 0x34);
    if (w < *reinterpret_cast<const int32_t*>(root + 0x28))
        w = *reinterpret_cast<const int32_t*>(root + 0x28);
    if (h < *reinterpret_cast<const int32_t*>(root + 0x2c))
        h = *reinterpret_cast<const int32_t*>(root + 0x2c);

    // Retail tests the LOW BYTE of _flags (`TEST R10B,0x30`), so only bits
    // 4 and 5 of the low byte take part.
    if ((flags & 0x30u) != 0) {
        const uint8_t* e = root;
        while (*reinterpret_cast<void* const*>(e + 0x98) != nullptr)
            e = *reinterpret_cast<const uint8_t* const*>(e + 0x98);   // _parent
        const uint8_t* dlg = *reinterpret_cast<const uint8_t* const*>(e + 0x90);
        if ((flags & 0x20u) != 0) {
            const float s = *reinterpret_cast<const float*>(
                *reinterpret_cast<const uint8_t* const*>(dlg + 0x10) + 0xdc);
            w = static_cast<int32_t>(
                static_cast<float>(*reinterpret_cast<const int32_t*>(device + 4)) * s);
        }
        if ((flags & 0x10u) != 0) {
            const float s = *reinterpret_cast<const float*>(
                *reinterpret_cast<const uint8_t* const*>(dlg + 0x10) + 0xdc);
            h = static_cast<int32_t>(
                static_cast<float>(*reinterpret_cast<const int32_t*>(device + 8)) * s);
        }
    }

    if (x < 0) x = 0;
    if (w + x > screenW) x = screenW - w;
    if (y < 0) y = 0;
    if (h + y > screenH) y = screenH - h;

    const int32_t dx = x - self->_upperLeftX;
    const int32_t dy = y - self->_upperLeftY;

    if (dx != 0 || dy != 0) {
        uint8_t* r = static_cast<uint8_t*>(self->_root);
        *reinterpret_cast<int32_t*>(r + 0x68) += dx;
        *reinterpret_cast<int32_t*>(r + 0x70) += dx;
        *reinterpret_cast<int32_t*>(r + 0x6c) += dy;
        *reinterpret_cast<int32_t*>(r + 0x74) += dy;

        using AdjustFn = void (*)(void*, int32_t, int32_t);
        static AdjustFn adjust = nullptr;
        if (!adjust) adjust = reinterpret_cast<AdjustFn>(
            ResolveAddr(Module::Runtime, 0x180115840));

        const int32_t n = *reinterpret_cast<const int32_t*>(r + 0xa8);
        for (int32_t i = 0; i < n; ++i) {
            void* child = (*reinterpret_cast<void** const*>(r + 0xa0))[i];
            if (child && adjust) adjust(child, dx, dy);
        }
        if (void* border = *reinterpret_cast<void* const*>(r + 0xb0))
            if (adjust) adjust(border, dx, dy);

        self->_upperLeftX = x;
        self->_upperLeftY = y;
    }

    RT_DIFF_WORK(dx != 0 || dy != 0);
}

// ============================================================================
// Registration
// ============================================================================
// Every port is registered TWICE — under its mangled name and under its retail
// address — even though every live call site today uses the name form. The
// address registration costs nothing and closes the Wave-5-lane-D failure mode
// where one live object ends up split between two implementations because a
// later call site used the other door.
//
// The names are repeated as literals here on purpose; see the note on the
// constants above.
// ============================================================================
// WAVE 10 LANE UIF
// ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z
//                                                 0x180115ad0  ord 2004
// ============================================================================
//
// "Stamp this serialized description onto that live element." It is the second
// half of `?CreateElement@Dialog@` (ported above by Wave 9 lane UIC): that one
// picks WHICH description wins, this one applies it.
//
// WHY IT WAS BLOCKED AND WHY IT IS NOT NOW. Wave 7 lane B held it in the
// "everything else that allocates" group at the top of this file. It does not,
// in fact, allocate anything of its own — but it FREES, twice, through the
// inlined `System::String` release, and every free in this project sat behind
// the Wave 2 allocator NO-GO. Wave 10 lane ALLOC retired that gate
// (docs/runtime_port/WAVE10_LANE_ALLOC.md): `BANISHED_RT_ALLOC=1` arms with the
// DLL present, so "our body frees what the DLL's unported code allocated" is
// one heap either way. This body does not need the flag; it frees through
// whichever `MemFree` is live.
//
// ---------------------------------------------------------------------------
// ABI. `QEBAX` returning void: RCX = `this` (const), RDX = the `InternalPtr<
// UI::Element>` argument. `InternalPtr<T>` is an 8-byte class with a
// non-trivial copy constructor, so MSVC x64 passes it BY ADDRESS even though
// the mangled name spells it by value — and the body proves it, dereferencing
// RDX eleven times (`MOV RBX,[RDI]` at 0x180115b15 and ten re-reads). So the
// second parameter is `void** elemPtr`, NOT `void* elem`.
// reference_hidden_retval_and_int_pointer_args.md is the family this belongs to.
//
// THE BODY, from the disassembly at 0x180115ad0 (0x31a bytes):
//
//   1. `elem->_name = this->GetPath().GetPathIdentifer();` through retail's
//      inlined String assign-with-refcount, then release both temporaries in
//      the order ident-then-path.
//   2. Eleven scalar copies from the description onto the element. Note that
//      `_flags` is OR-ed (0x180115bec `OR [RCX+0x34],EAX`), never assigned —
//      a description with no flags cannot CLEAR a flag the element already has
//      — and that `_id` (+0x3c) and `_tabOrder` (+0x38) CROSS OVER into
//      element +0x38 and +0x3c respectively. Both are retail behaviour, not
//      transcription slips, and both are the kind of thing a byte oracle
//      catches and a reading does not.
//   3. `_stringTable` -> `elem->_toolTip` (a `UIString` at +0x48: the table at
//      +0x48 and its index at +0x50), with the index taken from
//      `StringTable::GetIndexFromName` or -1.
//   4. `elem->_description = this` (+0xb8).
//   5. A `UI::Dialog` TEMPORARY on the stack, constructed and destroyed purely
//      to be the context argument of one virtual call: if `_childDesc` (+0x48)
//      is non-null, `elem->_border = *childDesc->vtbl[+0x40](childDesc, &slot,
//      &tempDialog)`, else `elem->_border = nullptr`.
//   6. A flags cascade guarded on four conditions — see the DEAD CODE note.
//
// ---------------------------------------------------------------------------
// THREE FACTS THAT ONLY THE DISASSEMBLY SHOWS
// (reference_ghidra_c_output_vs_disassembly.md)
// ---------------------------------------------------------------------------
//
// (a) THE INLINED `UI::Dialog` TEMPORARY IS THE EXPORTED CTOR/DTOR, INSTRUCTION
//     FOR INSTRUCTION. Retail inlined `??0Dialog@UI@@QEAA@XZ` (0x180112db0) and
//     `??1Dialog@UI@@UEAA@XZ` (0x180112e40) here. Both were disassembled and
//     compared against the inline before this port was written:
//       * the ctor is 3 vtable stores (Base 0x180154788, External 0x1801547a0,
//         Dialog 0x1801562a0) and 17 zero/constant stores out to +0x80, with
//         +0x30 = -1. The inline differs in ONE respect and it is not a
//         behavioural one: the inline writes +0x20/+0x28 as a single 16-byte
//         `XORPS/MOVDQA`, the out-of-line as two QWORDs. Same 16 bytes.
//       * the dtor is the four array destructors at +0x78/+0x68/+0x58/+0x48
//         (0x180009460, 0x180039440, 0x180039360, 0x180039280), the String
//         release at +0x28, and a tail-JMP to `~External` (0x1800bc950) — which
//         is exactly the tail of the inline, `~External` included.
//     So the port TRANSCRIBES the ctor (it is only constant stores, and doing
//     so adds NO callee at all) and CALLS the exported dtor. That is the same
//     out-of-line substitution `??1DialogInstance@` already makes for `MemFree`
//     in this file. It grows the cutover surface by exactly ONE named row,
//     `??1Dialog@UI@@UEAA@XZ` — reference_porting_can_grow_the_cutover_surface
//     .md says to say so rather than let a later census discover it.
//
// (b) `elem->_toolTip._index` IS WRITTEN AS A QWORD OVER AN UNINITIALISED
//     UPPER HALF. Retail stores the int32 index into a stack slot
//     (`MOV [RBP+0x1f],EAX`) and then reads EIGHT bytes back out of it
//     (`MOV RAX,[RBP+0x1f]; MOV [RCX+0x50],RAX`), so element +0x54..+0x57 —
//     the `UIString` pad — receives whatever was on retail's stack. That is
//     INDETERMINATE and cannot be reproduced. The port writes the DWORD only,
//     and the oracle EXCLUDES +0x54..+0x57 from its compare. This is a stated
//     deviation, not an accident, and it is the one place this body is not
//     byte-faithful.
//
// (c) THE FLAGS CASCADE IS DEAD CODE IN THE SHIPPED BINARY, and the port keeps
//     it anyway. The guard is
//         MOV EDX,[RCX+0x34] ; MOV EAX,EDX ; OR EAX,0x90000
//         MOV [RCX+0x34],EAX ; XOR EDX,EAX ; TEST DL,0x1 ; JZ over
//     `0x90000` touches bits 16 and 19 only, so `(old ^ (old|0x90000))` is
//     always ZERO in its low byte and the `JZ` is always taken. The parent walk
//     that follows (`OR [RCX+0x34],0x8000000` up the +0x98 chain) can never
//     execute. It is transcribed rather than deleted, because "provably
//     unreachable in this build" is a claim the port should not bake in.
//
// ---------------------------------------------------------------------------
// THE WIDER BLAST RADIUS — read this before registering
// ---------------------------------------------------------------------------
// This is one of the six names in this file whose registration also re-points
// an EXE PE-import slot: `standalone/address_registry.cpp:1115` binds
// 0x1401518d0 with `RegisterDispatchSlot(slot, ResolveExport(name))`, and
// `RuntimePortInit()` runs BEFORE `AddressRegistryInit()`. So registering this
// puts our body on the element-stamping path of every dialog the game builds,
// not merely on our own `fn_*` pointers. That is precisely the row the census
// asked for — it is *sourced* from address_registry.cpp — but it is why this
// one gets a REPLAY oracle rather than a shape check.
//
// EVERY CALLEE IS SATISFIED:
//   * 0x180006250 `?GetPath@External@Object@@`        — ported (external.cpp)
//   * 0x1800c6610 `?GetPathIdentifer@Filename@System@@` — ported (filename.cpp)
//   * 0x1800c7ad0 `MemFree`                            — ported, lane Q
//   * 0x180002470 `?GetIndexFromName@StringTable@UI@@`  — see sprite.cpp:571
//   * 0x180112e40 `??1Dialog@UI@@UEAA@XZ`              — NOT ported; new row
//   * `_childDesc`'s vtable slot 8                     — PURE on the base
//     (standalone/rt_data_mint_tables.inc, `RT_MINT_PURECALL`), so it is only
//     ever reached on a concrete derived description and porting this body
//     introduces no new cutover symbol for it — the same argument
//     `?CreateElement@Dialog@` already makes for the identical slot.
// ---------------------------------------------------------------------------

namespace {

// `??1Dialog@UI@@UEAA@XZ` 0x180112e40 — see fact (a). `UEAA`, the VIRTUAL
// spelling, is the one the export table carries (checked with pefile;
// reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
inline void DialogDtor(void* self) {
    RT_EXPORT_ONCE(VoidPtrFn, "??1Dialog@UI@@UEAA@XZ")(self);
}

// `?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z` 0x180002470.
using GetIndexFromNameFn = int32_t (*)(const void* table, const void* str);

// The retail-inlined `??0Dialog@UI@@QEAA@XZ`, transcribed. Constant stores
// only, so it adds no callee; the three vtables come out of the DLL's own
// export table rather than being minted (the `??_7Geometry@Graphics@@6B@` lever
// — a ported ctor must never put a second vtable in play).
constexpr size_t kDialogTempSize = 0x88;

void DialogTempCtor(uint8_t* p) {
    static void* const vtBase = ResolveExport("??_7Base@Object@@6B@");
    static void* const vtExt  = ResolveExport("??_7External@Object@@6B@");
    static void* const vtDlg  = ResolveExport("??_7Dialog@UI@@6B@");

    *reinterpret_cast<void**>(p + 0x00) = vtBase;
    *reinterpret_cast<void**>(p + 0x00) = vtExt;
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;
    // WAVE 10E lane FINAL: the stamp is the row's ONLY consumer here, so it is
    // the only place an instrument can ask whether anything ever dispatches
    // through it. Off (the default) `TableOrReal` returns `vtDlg` untouched.
    *reinterpret_cast<void**>(p + 0x00) = RtUiDlgTrip::TableOrReal(vtDlg);
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;   // the inline's 16-byte MOVDQA
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;   // covers these two together
    *reinterpret_cast<int32_t*>(p + 0x30)  = -1;
    *reinterpret_cast<int32_t*>(p + 0x34)  = 0;
    *reinterpret_cast<uint64_t*>(p + 0x38) = 0;
    *reinterpret_cast<int32_t*>(p + 0x40)  = 0;
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x80) = 0;

    RtUiDlgTrip::AfterStamp(p);   // no-op unless BANISHED_DLG_VPTR_TRIP=2
}

void EdCreateInstanceBody(const ElementDescription* self, void** elemPtr) {
    const uint8_t* d = reinterpret_cast<const uint8_t*>(self);

    // ---- 1. elem->_name = this->GetPath().GetPathIdentifer() --------------
    using GetPathFn  = void* (*)(const void* self, void* out);
    using GetIdentFn = void* (*)(void* self, void* out);
    static GetPathFn  getPath  = reinterpret_cast<GetPathFn>(
        ResolveExport("?GetPath@External@Object@@QEBA?AVFilename@System@@XZ"));
    static GetIdentFn getIdent = reinterpret_cast<GetIdentFn>(
        ResolveExport("?GetPathIdentifer@Filename@System@@QEBA?AV12@XZ"));

    void*  pathTmp  = nullptr;   // System::Filename — its first qword IS a String
    char*  identTmp = nullptr;
    char** idSlot   = nullptr;
    if (getPath && getIdent) {
        void* fn = getPath(self, &pathTmp);
        idSlot = static_cast<char**>(getIdent(fn, &identTmp));
    }

    uint8_t* elem = static_cast<uint8_t*>(*elemPtr);
    char*    want = idSlot ? *idSlot : nullptr;
    char*    old  = *reinterpret_cast<char**>(elem + 0x10);
    if (old != want) {
        if (old) {
            if (--RtSystem::StringRefCount(old) == 0) {
                // Retail's asymmetry: the FREEING branch leaves the FIELD
                // holding `data - 8` and frees that; the sharing branch nulls it.
                *reinterpret_cast<char**>(elem + 0x10) = old - 8;
                MemFreeStatic(*reinterpret_cast<char**>(elem + 0x10));
            } else {
                *reinterpret_cast<char**>(elem + 0x10) = nullptr;
            }
        }
        *reinterpret_cast<char**>(elem + 0x10) = want;
        if (want) ++RtSystem::StringRefCount(want);
    }

    ReleaseStringLocal(identTmp);
    ReleaseStringLocal(*reinterpret_cast<char**>(&pathTmp));

    // ---- 2. the eleven scalar copies --------------------------------------
    // `*elemPtr` is RE-READ before every store, exactly as retail does. Nothing
    // in between can change it, but the re-read is what the shipped bytes do
    // and transcribing it costs nothing.
    #define ED_ELEM  (static_cast<uint8_t*>(*elemPtr))
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x30) = *reinterpret_cast<const int32_t*>(d + 0x40); // _elementType -> _alignment
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x18) = *reinterpret_cast<const int32_t*>(d + 0x20);
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x1c) = *reinterpret_cast<const int32_t*>(d + 0x24);
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x20) = *reinterpret_cast<const int32_t*>(d + 0x28);
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x24) = *reinterpret_cast<const int32_t*>(d + 0x2c);
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x28) = *reinterpret_cast<const int32_t*>(d + 0x30);
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x2c) = *reinterpret_cast<const int32_t*>(d + 0x34);
    *reinterpret_cast<uint32_t*>(ED_ELEM + 0x34) |= *reinterpret_cast<const uint32_t*>(d + 0x44); // OR, never assign
    // NEGATIVE CONTROL 2 (`BANISHED_UIF_NEGCTRL=2`, declared in
    // runtime/src/ui/userinterface.cpp): drop the crossover, which is exactly
    // the kind of transcription slip a byte oracle exists to catch.
    if (RtUiUifNegCtrl() == 2) {
        *reinterpret_cast<int32_t*>(ED_ELEM + 0x38) = *reinterpret_cast<const int32_t*>(d + 0x38);
        *reinterpret_cast<int32_t*>(ED_ELEM + 0x3c) = *reinterpret_cast<const int32_t*>(d + 0x3c);
    } else {
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x38) = *reinterpret_cast<const int32_t*>(d + 0x3c);    // _id      -> +0x38
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x3c) = *reinterpret_cast<const int32_t*>(d + 0x38);    // _tabOrder-> +0x3c
    }
    *reinterpret_cast<uint64_t*>(ED_ELEM + 0x40) = *reinterpret_cast<const uint64_t*>(d + 0x50);

    // ---- 3. the tool-tip UIString -----------------------------------------
    void*   table = *reinterpret_cast<void* const*>(d + 0x58);
    int32_t index = -1;
    if (table) {
        static GetIndexFromNameFn gifn = reinterpret_cast<GetIndexFromNameFn>(
            ResolveExport("?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z"));
        if (gifn) index = gifn(table, d + 0x60);
    }
    elem = ED_ELEM;
    *reinterpret_cast<void**>(elem + 0x48)  = table;
    // Fact (b): retail writes a QWORD here whose upper half is uninitialised
    // stack. The port writes the DWORD only and the oracle excludes +0x54.
    *reinterpret_cast<int32_t*>(elem + 0x50) = index;

    // ---- 4. elem->_description = this -------------------------------------
    *reinterpret_cast<const void**>(ED_ELEM + 0xb8) = self;

    // ---- 5. the UI::Dialog temporary and the one virtual call -------------
    alignas(16) uint8_t tempDialog[kDialogTempSize];
    DialogTempCtor(tempDialog);

    void*  slot = nullptr;
    void** ret  = &slot;
    if (void* child = *reinterpret_cast<void* const*>(d + 0x48)) {
        using ChildFn = void** (*)(void* self, void** out, void* dialog);
        ChildFn f = reinterpret_cast<ChildFn>(
            (*reinterpret_cast<void***>(child))[8]);   // vtable +0x40
        ret = f(child, &slot, tempDialog);
    } else {
        slot = nullptr;
        ret  = &slot;
    }
    *reinterpret_cast<void**>(ED_ELEM + 0xb0) = *ret;

    // ---- 6. the flags cascade (fact (c): the inner walk is unreachable) ---
    elem = ED_ELEM;
    if (*reinterpret_cast<void* const*>(elem + 0x40) != nullptr &&
        *reinterpret_cast<void* const*>(elem + 0x48) != nullptr &&
        *reinterpret_cast<const int32_t*>(elem + 0x50) >= 0 &&
        (*reinterpret_cast<const uint32_t*>(elem + 0x34) & 0x800000u) == 0) {
        const uint32_t before = *reinterpret_cast<uint32_t*>(elem + 0x34);
        const uint32_t after  = before | 0x90000u;
        *reinterpret_cast<uint32_t*>(elem + 0x34) = after;
        if ((static_cast<uint8_t>(before ^ after) & 1u) != 0) {
            uint8_t* w = elem;
            do {
                *reinterpret_cast<uint32_t*>(w + 0x34) |= 0x8000000u;
                w = *reinterpret_cast<uint8_t**>(w + 0x98);
            } while (w != nullptr);
        }
    }

    #undef ED_ELEM

    // ---- the temporary's destructor ---------------------------------------
    DialogDtor(tempDialog);           // ~Dialog: see fact (a)
}

// ---- the replay oracle's storage ------------------------------------------
// A live 0xC0-byte Element snapshot plus the two masked copies the compare is
// made on. File scope for the same one-thread reason the rest of this file
// gives; CreateInstance is called from the dialog build, on MainLoop's thread.
alignas(16) uint8_t g_edCiScratch[0xC0];
alignas(16) uint8_t g_edCiOurs[0xC0];
alignas(16) uint8_t g_edCiTheirs[0xC0];

// The population counters the diff message reports, so a green result cannot be
// read as coverage it does not have
// (reference_probe_needs_an_effectiveness_control.md).
uint64_t g_edCiWithChild = 0;      // NOT replayed — see the oracle note
uint64_t g_edCiNoChild   = 0;      // replayed
uint64_t g_edCiWithTable = 0;      // reached StringTable::GetIndexFromName

} // namespace

// ORACLE: REPLAY, on the ELEMENT, with two things spelled out that a bare
// "replay" label would hide.
//
//  (i) IT IS SKIPPED WHEN `_childDesc` IS NON-NULL, and the count of skipped
//      calls is REPORTED. Slot 8 of a concrete description CREATES an element;
//      replaying it would build a second one, leak it, and then report a
//      "divergence" that is nothing but two different fresh addresses in
//      `elem->_border`. A replay that manufactures a divergence is worse than
//      no replay, and a replay whose uncovered fraction is invisible is the
//      reference_uncalled_port_is_not_verified.md failure one level up. So the
//      split is printed on every report: `nochild=N child=M table=K`.
//  (ii) THE STRING REFCOUNTS ARE COMPENSATED, because both bodies run the same
//      assign against the SAME two String buffers. Ours does (-1 old, +1 new)
//      on the live element; the DLL's does (-1 old, +1 new) on the scratch copy,
//      which holds the SAME pointers. Left alone that is a double release of
//      `old` — and the first of the two could free it out from under the
//      second. So the oracle takes a reference on `old` BEFORE either body runs
//      (making the net effect on it exactly -1 across the pair, with the free,
//      if any, happening at the very end) and drops one reference on the NEW
//      buffer afterwards. Both are undone before the caller sees anything.
//
// +0x54..+0x57 is excluded from the compare — fact (b) above. Both copies are
// built from the same live snapshot, so every other byte neither body writes is
// equal by construction rather than by luck
// (reference_oracle_must_not_compare_unwritten_bytes.md).
extern "C" void RtUiElementDescription_CreateInstance(const ElementDescription* self,
                                                      void** elemPtr) {
    RT_DIFF_BEGIN("UI::ElementDescription::CreateInstance");

    const uint8_t* d     = reinterpret_cast<const uint8_t*>(self);
    const bool hasChild  = (*reinterpret_cast<void* const*>(d + 0x48) != nullptr);
    const bool hasTable  = (*reinterpret_cast<void* const*>(d + 0x58) != nullptr);
    if (rt_diff_on__) {
        if (hasChild) ++g_edCiWithChild; else ++g_edCiNoChild;
        if (hasTable) ++g_edCiWithTable;
    }

    const bool replay = rt_diff_on__ && !hasChild && *elemPtr != nullptr;

    char* preRef = nullptr;
    if (replay) {
        uint8_t* live = static_cast<uint8_t*>(*elemPtr);
        std::memcpy(g_edCiScratch, live, sizeof(g_edCiScratch));
        preRef = *reinterpret_cast<char**>(live + 0x10);
        if (preRef) ++RtSystem::StringRefCount(preRef);   // note (ii)
    }

    EdCreateInstanceBody(self, elemPtr);

    if (replay) {
        void* scratchPtr = g_edCiScratch;
        using Fn = void (*)(const ElementDescription*, void**);
        RT_ORIGINAL_ONCE(Fn, kEdCreateInstance)(self, &scratchPtr);

        std::memcpy(g_edCiOurs,   *elemPtr,      sizeof(g_edCiOurs));
        std::memcpy(g_edCiTheirs, g_edCiScratch, sizeof(g_edCiTheirs));
        std::memset(g_edCiOurs   + 0x54, 0, 4);   // fact (b), indeterminate
        std::memset(g_edCiTheirs + 0x54, 0, 4);

        char ours[192], them[192];
        std::snprintf(ours, sizeof(ours),
                      "population nochild=%llu child=%llu table=%llu",
                      (unsigned long long)g_edCiNoChild,
                      (unsigned long long)g_edCiWithChild,
                      (unsigned long long)g_edCiWithTable);
        std::snprintf(them, sizeof(them), "%s", ours);
        RtDiffReport(&rt_diff_site__, true, ours, them);
        RtDiffReportBytes(&rt_diff_site__, g_edCiOurs, g_edCiTheirs,
                          sizeof(g_edCiOurs));

        // Undo note (ii): one reference back off the NEW buffer (both bodies
        // took one), and the pre-reference on `old` has already been consumed
        // by the DLL's own release.
        if (char* nw = *reinterpret_cast<char**>(
                static_cast<uint8_t*>(*elemPtr) + 0x10)) {
            if (--RtSystem::StringRefCount(nw) == 0) {
                MemFreeStatic(nw - 8);
            }
        }
    }
    RT_DIFF_WORK(true);
}

// ============================================================================
// THE TWIN-SYNTHETIC ORACLE for ?CreateInstance@ElementDescription
// ============================================================================
// THIS EXISTS BECAUSE THE REPLAY ORACLE ABOVE HAS NO POPULATION. Measured, not
// assumed: a `BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1` run to the pause menu
// registers 542 diff sites and `UI::ElementDescription::CreateInstance` is not
// among them — the body was never entered once. That is
// reference_uncalled_port_is_not_verified.md exactly, and a `divergences=0`
// from an absent site is not evidence of anything.
//
// THE REASON IS STRUCTURAL, not a gap in the harness. Grepping the tree for the
// row's callers finds exactly ONE:
//   code/Game/Component/frustumoutlinedescription.cpp:116 — the exe's
//   `FrustumOutlineDescription::CreateInstance` override calling its base
//   through `fn_ElementDescription_CreateInstance`, which reads exe PE-import
//   slot 0x1401518d0. A FrustumOutlineDescription is not instantiated on any
//   route this harness can drive. Ghidra reports NO xrefs to 0x180115ad0 at
//   all — every other caller is a virtual dispatch inside the DLL, which our
//   registration cannot reach anyway.
// So there is no run to wait for. The subject has to be built.
//
// THE SUBJECTS. One `UI::ElementDescription` per case, built with our own
// ported constructor on real allocator memory, and TWO zeroed 0xC0-byte
// `UI::Element`s — ours runs on A, the SHIPPED body on B, and all 0xC0 bytes
// are compared. Three cases, chosen so each covers a branch the others do not:
//
//   case 0  childDesc = null, stringTable = null, userData = null
//           -> the null-child arm; index = -1; the flags cascade's guard
//              short-circuits on its FIRST condition (elem+0x40 == 0)
//   case 1  childDesc = a SYNTHETIC description whose fake vtable slot 8 is a
//           recorder that writes a known sentinel into the out slot
//           -> the virtual arm, and `elem->_border` must equal that sentinel
//   case 2  stringTable = a SYNTHETIC UI::StringTable holding one entry whose
//           name is a synthetic System::String, `_textName` = that same String,
//           `_userData` non-null
//           -> `GetIndexFromName` returns 0, so ALL FOUR guard conditions pass
//              and `elem->_flags |= 0x90000` actually happens
//
// Every case also pre-sets both elements' `_name` to the SAME synthetic String
// with a refcount of 8, so the String-assign's RELEASE arm runs on both sides
// (4 releases across the six runs, none of them reaching zero).
//
// WHAT THIS ORACLE DOES NOT COVER, stated rather than implied:
//   * the String-assign's FREEING arm. It cannot be covered by a twin: at
//     refcount 1 the first body frees the buffer and the second would decrement
//     freed memory, and giving each side its own buffer makes the post-state
//     differ by POINTER VALUE and manufactures a divergence
//     (reference_control_must_pick_a_compared_subject.md). The freeing arm is
//     three instructions and is the same inline `ReleaseStringLocal` this file
//     already carries at four other sites.
//   * the case where `GetPath()` yields a non-empty path. These descriptions
//     have no path, so `want` is null on both sides. What that costs is the
//     ASSIGN arm (elem+0x10 = new + incref); the RELEASE arm above is covered.
//   * the flags cascade's inner parent walk — provably unreachable, fact (c).
namespace {

struct EdTwinCase {
    const char* name;
    void*  desc;
    void*  elemA;
    void*  elemB;
};

// The sentinel the fake child description hands back through the out slot.
void* const kEdTwinBorder = reinterpret_cast<void*>(static_cast<uintptr_t>(0xB07DE5));


// Fake `_childDesc` vtable slot 8 (+0x40): retail's shape is
// `InternalPtr<Element> slot8(void* out, const Dialog* ctx) const`, returning
// the out slot in RAX.
void** EdTwinChildSlot8(void* /*self*/, void** out, void* /*dialog*/) {
    *out = kEdTwinBorder;
    return out;
}
void* g_edTwinChildVtbl[9] = { nullptr, nullptr, nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr,
                               reinterpret_cast<void*>(&EdTwinChildSlot8) };

inline void* EdMemAlloc(size_t n, int align) {
    using Fn = void* (*)(size_t, int);
    return RT_EXPORT_ONCE(Fn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(n, align);
}

// A `System::String`: length dword at [p-8], refcount dword at [p-4], bytes at
// p. runtime/types/system/String.h.
char* EdMakeString(const char* text, int32_t refs) {
    const size_t len = std::strlen(text);
    uint8_t* base = static_cast<uint8_t*>(EdMemAlloc(len + 9, 8));
    if (!base) return nullptr;
    *reinterpret_cast<int32_t*>(base + 0) = static_cast<int32_t>(len);
    *reinterpret_cast<int32_t*>(base + 4) = refs;
    std::memcpy(base + 8, text, len + 1);
    return reinterpret_cast<char*>(base + 8);
}

void VerifyElementDescriptionCreateInstanceTwin() {
    if (!RtDiffEnabled()) return;
    static RtDiffSite site = { "UI::ElementDescription::CreateInstance [twin synthetic]" };
    RtDiffRegister(&site);

    using Fn = void (*)(const ElementDescription*, void**);
    // RT_RAW_OK: the ORACLE half must reach the SHIPPED body, never our own
    // registration (WAVE9_LANE_FNHOOK.md's self-comparing detours).
    Fn theirs = reinterpret_cast<Fn>(ResolveExportRaw(kEdCreateInstance));
    if (!theirs) {
        RtDiffReport(&site, false, "twin subjects built",
                     "shipped ?CreateInstance@ElementDescription not resolvable");
        return;
    }

    // The shared name String: refcount 8 so all six release-arm executions stay
    // in the SHARING branch and nothing is freed under either body.
    char* shared = EdMakeString("uif_twin", 8);

    // The synthetic StringTable for case 2: {..., data @ +0x20, count @ +0x28}.
    uint8_t* table = static_cast<uint8_t*>(EdMemAlloc(0x30, 8));
    char*    key   = EdMakeString("uif_key", 8);
    StringTableTextEntry* entries =
        static_cast<StringTableTextEntry*>(EdMemAlloc(sizeof(StringTableTextEntry), 8));
    bool built = shared && table && key && entries;
    if (built) {
        std::memset(table, 0, 0x30);
        entries[0].name = key;
        entries[0].text = nullptr;
        *reinterpret_cast<void**>(table + StringTableOffsets::kData)   = entries;
        *reinterpret_cast<int32_t*>(table + StringTableOffsets::kCount) = 1;
    }

    // The fake child description: one qword, its vtable.
    void** child = static_cast<void**>(EdMemAlloc(8, 8));
    if (child) *child = g_edTwinChildVtbl;
    built = built && child;

    int cases = 0, mismatched = 0, borderOk = 0, cascadeRan = 0;

    for (int c = 0; built && c < 3; ++c) {
        ElementDescription* desc =
            static_cast<ElementDescription*>(EdMemAlloc(sizeof(ElementDescription), 8));
        uint8_t* a = static_cast<uint8_t*>(EdMemAlloc(0xC0, 16));
        uint8_t* b = static_cast<uint8_t*>(EdMemAlloc(0xC0, 16));
        if (!desc || !a || !b) break;

        RtUiElementDescription_Ctor(desc);        // our own ported ctor
        uint8_t* d = reinterpret_cast<uint8_t*>(desc);
        // Distinct, recognisable scalars so a swapped field is visible.
        *reinterpret_cast<int32_t*>(d + 0x20) = 0x11;   // _x
        *reinterpret_cast<int32_t*>(d + 0x24) = 0x22;
        *reinterpret_cast<int32_t*>(d + 0x28) = 0x33;
        *reinterpret_cast<int32_t*>(d + 0x2c) = 0x44;
        *reinterpret_cast<int32_t*>(d + 0x30) = 0x55;
        *reinterpret_cast<int32_t*>(d + 0x34) = 0x66;
        *reinterpret_cast<int32_t*>(d + 0x38) = 0x77;   // _tabOrder -> elem+0x3c
        *reinterpret_cast<int32_t*>(d + 0x3c) = 0x88;   // _id       -> elem+0x38
        *reinterpret_cast<uint32_t*>(d + 0x40) = 0x99;  // _elementType
        *reinterpret_cast<uint32_t*>(d + 0x44) = 0x0000a000u;   // _flags, OR-ed
        if (c == 1) *reinterpret_cast<void**>(d + 0x48) = child;
        if (c == 2) {
            *reinterpret_cast<void**>(d + 0x50) = reinterpret_cast<void*>(0x1234);
            *reinterpret_cast<void**>(d + 0x58) = table;
            *reinterpret_cast<char**>(d + 0x60) = key;
        }

        std::memset(a, 0, 0xC0);
        std::memset(b, 0, 0xC0);
        *reinterpret_cast<char**>(a + 0x10) = shared;   // the release arm
        *reinterpret_cast<char**>(b + 0x10) = shared;

        void* pa = a;
        void* pb = b;
        RtUiElementDescription_CreateInstance(desc, &pa);
        theirs(desc, &pb);

        // +0x54..+0x57 is the UIString pad retail fills from uninitialised
        // stack — fact (b). Zeroed on both sides before the compare.
        std::memset(a + 0x54, 0, 4);
        std::memset(b + 0x54, 0, 4);
        if (std::memcmp(a, b, 0xC0) != 0) ++mismatched;
        if (c == 1 && *reinterpret_cast<void**>(a + 0xb0) == kEdTwinBorder) ++borderOk;
        if (c == 2 && (*reinterpret_cast<uint32_t*>(a + 0x34) & 0x90000u) == 0x90000u)
            ++cascadeRan;
        ++cases;

        char ours[160], them[160];
        std::snprintf(ours, sizeof(ours), "case %d (%s)", c,
                      c == 0 ? "no child, no table"
                             : (c == 1 ? "child virtual" : "table + cascade"));
        std::snprintf(them, sizeof(them), "%s", ours);
        RtDiffReport(&site, true, ours, them);
        RtDiffReportBytes(&site, a, b, 0xC0);
    }

    // The EFFECTIVENESS CONTROL. A green byte compare over three cases that all
    // took the same arm would read identically to a real pass, so the two facts
    // that distinguish the arms are asserted by name:
    //   border=1  the child virtual actually ran and its sentinel arrived
    //   cascade=1 all four guard conditions passed and the flags OR happened
    char ours[224], them[224];
    std::snprintf(ours, sizeof(ours),
                  "cases=3 border=1 cascade=1 mismatched=0");
    std::snprintf(them, sizeof(them),
                  "cases=%d border=%d cascade=%d mismatched=%d",
                  cases, borderOk, cascadeRan, mismatched);
    RtDiffReport(&site, cases == 3 && borderOk == 1 && cascadeRan == 1 &&
                        mismatched == 0, ours, them);
}

} // namespace

int RtUiDialogRegister() {
    int n = 0;

    // ---- UI::DialogInstance ----
    RegisterRuntimeExport(
        "?GetRoot@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ",
        (void*)&RtUiDialogInstance_GetRoot);
    RegisterRuntimeAddr(0x180045580, (void*)&RtUiDialogInstance_GetRoot);              ++n;

    RegisterRuntimeExport(
        "?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z",
        (void*)&RtUiDialogInstance_GetController);
    RegisterRuntimeAddr(0x180113320, (void*)&RtUiDialogInstance_GetController);        ++n;

    RegisterRuntimeExport(
        "?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z",
        (void*)&RtUiDialogInstance_FindElement);
    RegisterRuntimeAddr(0x180114c70, (void*)&RtUiDialogInstance_FindElement);          ++n;

    RegisterRuntimeExport(
        "?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z",
        (void*)&RtUiDialogInstance_FindElementType);
    RegisterRuntimeAddr(0x1801140c0, (void*)&RtUiDialogInstance_FindElementType);      ++n;

    RegisterRuntimeExport("?SetFlagsOnOff@DialogInstance@UI@@QEAAXII@Z",
                          (void*)&RtUiDialogInstance_SetFlagsOnOff);
    RegisterRuntimeAddr(0x180114690, (void*)&RtUiDialogInstance_SetFlagsOnOff);        ++n;

    RegisterRuntimeExport("?StopTimer@DialogInstance@UI@@QEAAXH@Z",
                          (void*)&RtUiDialogInstance_StopTimer);
    RegisterRuntimeAddr(0x180113730, (void*)&RtUiDialogInstance_StopTimer);            ++n;

    RegisterRuntimeExport("?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ",
                          (void*)&RtUiDialogInstance_OnUpdateLayout);
    RegisterRuntimeAddr(0x1801153a0, (void*)&RtUiDialogInstance_OnUpdateLayout);       ++n;

    RegisterRuntimeExport("?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z",
                          (void*)&RtUiDialogInstance_OnUpdate);
    RegisterRuntimeAddr(0x180115400, (void*)&RtUiDialogInstance_OnUpdate);             ++n;

    RegisterRuntimeExport(
        "?OnElementEvent@DialogInstance@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z",
        (void*)&RtUiDialogInstance_OnElementEvent);
    RegisterRuntimeAddr(0x180114ed0, (void*)&RtUiDialogInstance_OnElementEvent);       ++n;

    // ---- UI::Dialog ----
    RegisterRuntimeExport(
        "?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z",
        (void*)&RtUiDialog_GetTemplate);
    RegisterRuntimeAddr(0x1800388b0, (void*)&RtUiDialog_GetTemplate);                  ++n;

    // ---- UI::DialogController ----
    RegisterRuntimeExport(
        "?GetContainer@DialogController@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ",
        (void*)&RtUiDialogController_GetContainer);
    RegisterRuntimeAddr(0x18003b700, (void*)&RtUiDialogController_GetContainer);       ++n;

    RegisterRuntimeExport("??1DialogController@UI@@UEAA@XZ",
                          (void*)&RtUiDialogController_Dtor);
    RegisterRuntimeAddr(0x1800cd2f0, (void*)&RtUiDialogController_Dtor);               ++n;

    // ---- UI::ElementDescription ----
    RegisterRuntimeExport("??0ElementDescription@UI@@QEAA@XZ",
                          (void*)&RtUiElementDescription_Ctor);
    RegisterRuntimeAddr(0x180115a40, (void*)&RtUiElementDescription_Ctor);             ++n;

    RegisterRuntimeExport("?GetFileVersion@ElementDescription@UI@@UEBAHXZ",
                          (void*)&RtUiElementDescription_GetFileVersion);
    RegisterRuntimeAddr(0x180116af0, (void*)&RtUiElementDescription_GetFileVersion);   ++n;

    // WAVE 9 LANE UIF — a live minted-vtable row: `??_7ElementDescription` slot
    // 0 is lane K's ported deleting destructor, which reaches this by address.
    RegisterRuntimeExport("??1ElementDescription@UI@@UEAA@XZ",
                          (void*)&RtUiElementDescription_Dtor);
    RegisterRuntimeAddr(0x180116100, (void*)&RtUiElementDescription_Dtor);            ++n;

    // ---- Wave 9 lane Z: three DialogInstance vtable slots ------------------
    // Registered by NAME and by ADDRESS like the rest of the lane. The name is
    // what the exe-side vtable builders ask for; the address is what the minted
    // `??_7DialogInstance@UI@@6B@` looks up through RtPortLookupAddr.
    RegisterRuntimeExport("?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtUiDialogInstance_Persist);
    RegisterRuntimeAddr(0x180114fa0, (void*)&RtUiDialogInstance_Persist);              ++n;

    RegisterRuntimeExport("?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtUiDialogInstance_OnSerializeSettings);
    RegisterRuntimeAddr(0x180115470, (void*)&RtUiDialogInstance_OnSerializeSettings);  ++n;

    RegisterRuntimeExport("?TypeName@DialogInstance@UI@@UEBAPEBDXZ",
                          (void*)&RtUiDialogInstance_TypeName);
    RegisterRuntimeAddr(0x180045400, (void*)&RtUiDialogInstance_TypeName);             ++n;

    // ---- Wave 10B lane DLGV: the fourth DialogInstance vtable slot ---------
    // Same two doors, same reasons: the name is what the exe-side dialog vtable
    // builders in code/Game/Dialog/ and code/Game/Player/tool_base_toolui.cpp
    // ask for (all three already name `?Type@DialogInstance@UI@@UEBAIXZ`), and
    // the address is what the minted `??_7DialogInstance@UI@@6B@` looks up.
    RegisterRuntimeExport("?Type@DialogInstance@UI@@UEBAIXZ",
                          (void*)&RtUiDialogInstance_Type);
    RegisterRuntimeAddr(0x1800453e0, (void*)&RtUiDialogInstance_Type);                 ++n;

    // ---- Wave 9 lane UID: the teardown half of the dialog lifecycle -------
    // Registered under BOTH doors, which is the convention lane UIF's
    // half-registered `?SetUpperLeft@` proved is not decoration:
    //   * BY NAME  -- engine/funcs/engine_funcs.cpp:1757 binds
    //     `fn_DialogInstance_Dtor` through ResolveExport.
    //   * BY ADDRESS -- runtime/src/object/deleting_dtor.cpp's RT_DELDTOR row
    //     for UI::DialogInstance (0x1800458c0 -> 0x180114d40) reaches ~this
    //     by address, and the minted `??_7DialogInstance@UI@@6B@` fills its
    //     slot 0 from that deleting destructor.
    // The mangling is the VIRTUAL one, UEAA, taken from the shipped export
    // directory rather than constructed: QEAA would resolve NULL in silence
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
    RegisterRuntimeExport("??1DialogInstance@UI@@UEAA@XZ",
                          (void*)&RtUiDialogInstance_Dtor);
    RegisterRuntimeAddr(0x180114d40, (void*)&RtUiDialogInstance_Dtor);                ++n;

    // The IDENTITY LINE for this lane. Every measurement in
    // docs/runtime_port/WAVE9_LANE_UID.md is gated on finding this string in
    // THAT RUN'S OWN log, not on "I rebuilt" and not on a timestamp -- lanes
    // in this wave have measured the wrong exe more than once.
    printf("[RtPort]   WAVE9 LANE UID ARMED -- ~DialogInstance (0x180114d40) + "
                "LinkedList::Clear (0x1800325f0) + the Array<8>::SetSize address door "
                "(0x18002fca0). Shape oracles report under BANISHED_RT_DIFF=1; read "
                "the [negative control] line first.\n");

    // ---- Wave 9 lane UIC: the two rows lane UIF declined at 8 and 11 -------
    // Both names were verified present in the shipped export directory with
    // pefile BEFORE the bodies were written (tools/uic_probe.py), and both are
    // registered under BOTH doors: `?CreateElement@` is reached by NAME from
    // engine/funcs/engine_funcs.cpp:1959 and by DISPATCH SLOT from
    // standalone/address_registry.cpp:1414, and `?SetUpperLeft@` by NAME from
    // engine/funcs/engine_funcs.cpp:2932.
    RegisterRuntimeExport(
        "?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@"
        "V?$ExternalPtr@VElementDescription@UI@@@4@@Z",
        (void*)&RtUiDialog_CreateElement);
    RegisterRuntimeAddr(0x180112ed0, (void*)&RtUiDialog_CreateElement);                ++n;

    RegisterRuntimeExport("?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z",
                          (void*)&RtUiDialogInstance_SetUpperLeft);
    RegisterRuntimeAddr(0x180114a60, (void*)&RtUiDialogInstance_SetUpperLeft);         ++n;

    // ---- Wave 10 lane UIF --------------------------------------------------
    // Registering this ALSO re-points exe PE-import slot 0x1401518d0 (see "THE
    // WIDER BLAST RADIUS" at the body). Name checked against the shipped export
    // directory with pefile: 0x115ad0 / ordinal 2004.
    RegisterRuntimeExport(
        "?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z",
        (void*)&RtUiElementDescription_CreateInstance);
    RegisterRuntimeAddr(0x180115ad0, (void*)&RtUiElementDescription_CreateInstance);   ++n;

    // ---- the shape oracle, run once, here ----------------------------------
    // These four cannot be replayed (running them twice would tick every
    // controller twice, or swap-remove the object twice). Each check appears in
    // RtDiffSummary() as a single call — that is one check, not one per
    // invocation, and WAVE7_LANE_B.md §4 says so explicitly so that a `calls=1`
    // row is not misread as "barely exercised".
    {
        static RtDiffSite sLayout = { "UI::DialogInstance::OnUpdateLayout[shape]",  0, 0, false, false };
        static RtDiffSite sUpdate = { "UI::DialogInstance::OnUpdate[shape]",        0, 0, false, false };
        static RtDiffSite sEvent  = { "UI::DialogInstance::OnElementEvent[shape]",  0, 0, false, false };
        static RtDiffSite sDtor   = { "UI::DialogController::~DialogController[shape]", 0, 0, false, false };
        VerifyDispatchLoop(&sLayout, "?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ",
                           RtUI::kControllerVtblOnUpdateLayout);
        VerifyDispatchLoop(&sUpdate, "?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z",
                           RtUI::kControllerVtblOnUpdate);
        VerifyDispatchLoop(&sEvent,
                           "?OnElementEvent@DialogInstance@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z",
                           RtUI::kControllerVtblOnElementEvent);
        VerifyDtorThunk(&sDtor, "??1DialogController@UI@@UEAA@XZ",
                        // thunk bytes, so it must see the shipped vtable and dies with the oracle.
                        // RT_RAW_OK: an ORACLE read -- VerifyDtorThunk compares the DLL's own
                        ResolveExportRaw("??_7DialogController@UI@@6B@"),
                        ResolveExportRaw("??1Internal@Object@@UEAA@XZ"));

        // ---- Wave 9 lane UID ----------------------------------------------
        // Two decoders and ONE NEGATIVE CONTROL THAT RUNS. Read the control
        // line first: if it says a decoder failed to REJECT a body that is not
        // its own, the two green lines above it mean nothing
        // (reference_probe_needs_an_effectiveness_control.md).
        static RtDiffSite sDlgDtor = { "UI::DialogInstance::~DialogInstance[shape]", 0, 0, false, false };
        static RtDiffSite sLlClear = { "Collection::LinkedList::Clear(0x1800325f0)[shape]", 0, 0, false, false };
        VerifyDialogInstanceDtorShape(&sDlgDtor);
        VerifyListClearShape(&sLlClear);
        VerifyLaneUidControls();
    }

    return n;
}
