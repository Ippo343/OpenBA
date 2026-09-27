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
//     invocation — say so when reading the summary rather than concluding the
//     function was never exercised.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/system/Filename.h"
#include "../../types/ui/Dialog.h"
#include "../../types/ui/DialogInstance.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/ui/StringTable.h"   // WAVE 10 LANE UIF — the twin's table
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_Event30
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

// ResolveAddr, same reasoning and the same "must stay byte-identical to
// resolve.h's" contract. WAVE 9 LANE UIC needs it for one unexported-by-us edge
// (`?AdjustUpperLeft@Element@UI@@`, reached through the seam so that
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

namespace {

// ============================================================================
// Mangled names.
//
// The registrations at the bottom of this file deliberately repeat each name
// as its own string literal instead of using these constants:
// tools/runtime_census.py cannot see through a NAMED CONSTANT and would
// under-count the lane, which is how Wave 3's Object count stood at 21/41 when
// it was really 27/41 (reference_census_cannot_see_nonliteral_registrations.md).
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
// (ResolveExport) so that if lane A registers a port of
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

// ---- WAVE 27 lane IOTEXT: the same exported DATA slot, for ----------------
// `??_7ElementDescription@UI@@6B@` slots 1 and 2.
//
// `?TypeName@ElementDescription@UI@@UEBAPEBDXZ` (0x1800350f0) is
//     MOV RAX,qword ptr [0x1801fab00] ; RET
// and `?Type@ElementDescription@UI@@UEBAIXZ` (0x1800350d0) is
//     MOV EAX,dword ptr [0x1801faaf8] ; RET
// -- the identical two-slot / one-object shape `RtUiDialogInstance_TypeName`
// documents below, eight bytes apart on ONE BaseTypeInfo. Read out of the
// RETAIL DLL's disassembly (Runtime-x64.dll), not the profile build.
//
// UNLIKE the DialogInstance row's ⚠ note, this symbol IS in the mint today:
// standalone/rt_data_mint_tables.inc:1989 carries
// `?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A` at 0x1801faaf8.
// Same door, same retry-a-miss form, so a row minted later cannot be frozen
// null by whoever resolves first.
inline const uint8_t* ElementDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A");
}


// ---- WAVE 9 LANE UID helpers ----------------------------------------------
// All four use the RETRY-a-miss form (RT_DATA_SLOT / RT_EXPORT_ONCE, and the
// hand-written equivalent for the one address door), not the frozen
// function-local static this file uses elsewhere. That is lane E3's finding
// applied rather than repeated: 22 sites in the tree let the FIRST caller fix
// a null for the life of the process, and `~DialogInstance` runs during
// teardown, which is exactly when an ordering assumption is least safe.

using DeletingDtorFn = void* (*)(void* self, unsigned flags);
using VoidPtrFn      = void  (*)(void* p);

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

// ENDGAME B.3.2 step 2 — the 0x1800325f0 forwarder that used to sit here is
// GONE. `self->_children` (DialogInstance.h) is a real
// `Collection::LinkedList<InternalBasePtr>` member now, so its one caller
// below calls `.Clear()` on the container directly.

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
// and no oracle site is manufactured for the sake of a green count over work
// that never happened
// (reference_probe_needs_an_effectiveness_control.md).
// ENDGAME B.3.9 UI-D: the two RET slot doors are retired with the minted
// table; the members are `DialogInstance::Persist` / `OnSerializeSettings`.

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
// (ENDGAME B.3.9 UI-D: the door is retired; the member is below.)

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
    new (out) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::DialogInstance*>(self)         // B.3.6 bridge
            ->UI::DialogInstance::GetRoot());
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
    new (out) Object::InternalPtr<UI::DialogController>(
        reinterpret_cast<const UI::DialogInstance*>(self)   // B.3.6 bridge
            ->UI::DialogInstance::GetController(typeId));
    return out;
}

// ?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z
// Retail 0x180114c70, ordinal 2346. A forwarder:
//   MOV RCX,[RCX+0x50] / CALL Element::FindElement / MOV RAX,RBX / RET
// NO NULL CHECK on _root — retail has none, and CLAUDE.md's rule applies: a
// guard here would not restore the behaviour, it would skip it. If this faults
// the dialog has no root element and THAT is the bug.
void** RtUiDialogInstance_FindElement(DialogInstance* self, void** out, const void* name) {
    new (out) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::DialogInstance*>(self)         // B.3.6 bridge
            ->UI::DialogInstance::FindElement(*static_cast<const System::String*>(name)));
    return out;
}

// ?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z
// Retail 0x1801140c0, ordinal 2348. The same forwarder against
// Element::FindElementType. Same absent null check, same reason.
void** RtUiDialogInstance_FindElementType(DialogInstance* self, void** out, uint32_t typeId) {
    new (out) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::DialogInstance*>(self)         // B.3.6 bridge
            ->UI::DialogInstance::FindElementType(typeId));
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
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::SetFlagsOnOff(on, off);
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
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::StopTimer(id);
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

// ?OnUpdateLayout@ (0x1801153a0), ?OnUpdate@ (0x180115400) and
// ?OnElementEvent@ (0x180114ed0): ENDGAME B.3.9 UI-D retired the three slot
// doors with the minted table; the members are in the UI section below.
// OnElementEvent's InternalPtr<Element> arrives BY ADDRESS and the member
// hands each controller a pointer to its OWN fresh copy, never the caller's.

// ============================================================================
// UI::DialogInstance — frame leftovers (docs/runtime_port/closure/WORKLIST.md)
// ============================================================================
// Four bodies read from the disassembly (not the decompiled C, which drops the
// implicit RDX argument on the `OnUpdate` vtable call and mis-shapes
// `ComputeLayout`'s tail-call jump table — reference_ghidra_c_output_vs_
// disassembly.md). Raw Element offsets are spelled out at the use site, same
// convention runtime/types/ui/UserInterface.h states for a foreign class this
// file does not own.
//
// Element vtable slots used below (runtime/types/ui/Element.h): 0x20
// ComputeSize(), 0x28 ComputeLayout(x,y,w,h), 0x38 Update(...), 0x48
// Contains(x,y,UserInterface*). DialogInstance's OWN vtable slots (this file's
// tables above / tooltipdialog.cpp): 0x30 OnUpdateLayout(), 0x38 OnUpdate(time).

// ?Contains@DialogInstance@UI@@QEBA_NHH@Z — retail 0x180114820, ord 1861.
// Disassembly: flags bit2 -> always false; flags bit3 -> TAIL CALL into the
// root Element's own Contains (vtable slot 9/+0x48) with (x, y, this->_ui);
// otherwise a plain inclusive bounds test against the root's layout rect.
bool RtUiDialogInstance_Contains(const DialogInstance* self, int32_t x, int32_t y) {
    return reinterpret_cast<const UI::DialogInstance*>(self)   // B.3.6 bridge
        ->UI::DialogInstance::Contains(x, y);
}

// ?GetMouseCursorIndex@DialogInstance@UI@@QEBAHV?$InternalPtr@VElement@UI@@@Object@@HH@Z
// Retail 0x180114010, ord 2798. Recurses depth-first over `elem`'s children
// (Element::_children, +0xa0/+0xa8), keeping `elem`'s own cursor index
// (+0x3c) as the default and overriding it with whichever child's bounds
// [left,right) x [top,bottom) (+0x68/+0x70/+0x6c/+0x74, HALF-open, unlike
// Contains' inclusive test above) contains (x,y) and whose flags low two bits
// (+0x34 & 3) are both clear.
extern "C" int32_t RtUiDialogInstance_GetMouseCursorIndex(const DialogInstance* self,
                                               void** elementPtr,
                                               int32_t x, int32_t y) {
    // The InternalPtr arrives BY ADDRESS (MSVC's by-value class ABI); the
    // member takes it by value (a non-virtual member: GCC at both ends).
    return reinterpret_cast<const UI::DialogInstance*>(self)   // B.3.6 bridge
        ->UI::DialogInstance::GetMouseCursorIndex(
            Object::InternalPtr<UI::Element>(static_cast<UI::Element*>(*elementPtr)), x, y);
}

// ?ComputeLayout@DialogInstance@UI@@QEAAXHHHH@Z — retail 0x180113d90, ord 1760.
// Byte-faithful transcription of the disassembly (the decompiled C mis-shapes
// the tail-call jump table into a 4-arg call and drops the 5th, stack-passed
// argument -- see 0x180113fac's "WARNING: Could not recover jumptable").
//   1. this->OnUpdateLayout() (own vtable slot 6/+0x30) — no extra args.
//   2. root->ComputeSize(); clamp (x,y) into [0,w)x[0,h) against the root's
//      computed size plus this->_upperLeftX/Y, then place the root inside the
//      remaining space per its alignment (Element::_alignment, 9-way grid)
//      and fill flags (Element::_flags bits 2/3).
//   3. root->ComputeSize() AGAIN (retail calls it twice; reproduced even
//      though it looks redundant -- byte-faithful first). Compute the root's
//      total extent (layout size + padding, floored to its min size), and if
//      flags bit 4 or 5 (0x30) is set, walk to the OUTERMOST ancestor via
//      Element::_parent (+0x98) and re-derive width/height from
//      Video::Device::_singleton's screen size times that ancestor's
//      DialogInstance's UserInterface::_uiScale (bit 5 -> width, bit 4 ->
//      height).
//   4. TAIL-CALL root->ComputeLayout(x, y, x+w, y+h) (vtable slot 5/+0x28).
void RtUiDialogInstance_ComputeLayout(DialogInstance* self, int32_t x, int32_t y,
                                      int32_t w, int32_t h) {
    // B.3.6: the body is UI::DialogInstance::ComputeLayout (bottom of file).
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::ComputeLayout(x, y, w, h);
}

// ?Update@DialogInstance@UI@@QEAAXAEBVTime@System@@PEAVUserInterface@2@AEAUUpdateState@Element@2@@Z
// Retail 0x180113980, ord 4483. Read from the disassembly for one detail the
// decompiler drops: the `OnUpdate` vtable call at +0x38 implicitly reuses RDX
// (this function's own `time` argument, never reloaded between entry and the
// call), i.e. it is `this->OnUpdate(time)`, not a bare `this->OnUpdate()`.
//
//   1. If ANY child dialog has flags bit1 set, skip straight to step 5 (the
//      recursive per-child Update sweep) -- this dialog's own frame is
//      suppressed while a child claims that bit.
//   2. Otherwise: this->OnUpdate(time) (own vtable slot 7/+0x38). Then, unless
//      UpdateState bytes +7/+8/+9 say a drag/other block is active: if
//      UpdateState+0x20 is clear and Contains(mouseX,mouseY), accumulate hover
//      time once the mouse has settled at the same position, arming
//      `_hovering` past the tooltip delay; otherwise (some blocking bit set)
//      and `_hovering` was set, clear it and push a "hover end" event (type
//      0x11) via `UI::UserInterface::_events`.
//   3. Walk `_timers` (stride 0x18): decrement the LIVE countdown at +0x08 —
//      *** CORRECTION: DialogInstance.h's own comment calls +0x08 "_period,
//      the reload value" and +0x00 "_remaining". This body proves the
//      opposite is the live field: it is +0x08 that ticks down and fires the
//      event, so +0x08 is the countdown and +0x00 the untouched reload copy.
//      Both start equal (StartTimer writes the same value into both), which
//      is why no consumer noticed until this port read the decrement. ***
//      When a timer's countdown reaches <=0, push a "timer fired" event
//      (type 0x13, data = the timer's +0x10 id) and swap the remaining
//      timers down by one slot (retail does NOT re-visit the slot the swap
//      just filled; reproduced as-is).
//   4. root->Update(time, ui, &topDialog, updateState) (Element vtable slot
//      7/+0x38, five arguments -- the decompiler shows only four; the fifth,
//      UpdateState*, is passed on the stack, read from 0x180113ad9's
//      `MOV [RSP+0x20],RDI`). Then, unless flags bit2 (mouse passthrough) is
//      set -- in which case the whole hit block is SKIPPED: if flags bit3 is set, ask the root's own
//      Contains(x,y,_dialog) [Element vtable slot 9]; otherwise a bounds test
//      against the root's layout rect. On a miss, skip straight to step 5.
//      On a hit, push up to three UI events (types 8/9/0xa) for
//      UpdateState's own event bits (+1,+3,+2) with the mouse position
//      (UpdateState+0x18) as payload, then clear those UpdateState bits and
//      set UpdateState+0x20.
//   5. Recurse: call this same function on every child dialog in `_children`.
void RtUiDialogInstance_Update(DialogInstance* self, const void* time,
                               void* ui, uint8_t* updateState) {
    // B.3.6: the body is UI::DialogInstance::Update (bottom of file).
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::Update(*static_cast<const System::Time*>(time),
                                     static_cast<UI::UserInterface*>(ui), updateState);
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
    new (out) Object::ExternalPtr<UI::ElementDescription>(
        reinterpret_cast<const UI::Dialog*>(self)           // B.3.6 bridge
            ->UI::Dialog::GetTemplate(index));
    return out;
}

// ============================================================================
// UI::DialogController
// ============================================================================

// ?GetContainer@DialogController@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ
// Retail 0x18003b700, ordinal 2508. The same one-liner as GetRoot, at +0x20.
void** RtUiDialogController_GetContainer(DialogController* self, void** out) {
    new (out) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::DialogController*>(self)       // B.3.6 bridge
            ->UI::DialogController::GetContainer());
    return out;
}

// ??1DialogController@UI@@UEAA@XZ (0x1800cd2f0): the door is RETIRED (ENDGAME
// B.3.9 UI-D) — its only reader was the RT_DELDTOR row of the retired minted
// `??_7DialogController@UI@@6B@`; the empty `UI::DialogController::~DialogController`
// is retail's body.


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
//     is ever in play and it is whichever one the object-table port
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
// ENDGAME B.3.9 UI-D: the door `RtUiDialogInstance_Dtor` that stood here is
// retired with the minted table and its RT_DELDTOR row; the body above is
// `UI::DialogInstance::~DialogInstance` (ReleaseFields, the UI section), and
// ~Internal is the compiler's chain.

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

    static void* const vtblBase = ResolveExport(RtUI::kVtblBase);
    static void* const vtblExt  = ResolveExport(RtUI::kVtblExternal);
    static void* const vtblEd   = ResolveExport(RtUI::kVtblElementDescription);

    alignas(16) static uint8_t scratch[sizeof(ElementDescription)];

    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    *reinterpret_cast<void**>(p + 0x00) = vtblBase;   // Object::Base
    *reinterpret_cast<void**>(p + 0x00) = vtblExt;    // Object::External
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;       // External::_path (Filename)
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;       // External::_refs data
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;       // External::_refs size/capacity
    *reinterpret_cast<void**>(p + 0x00) = vtblEd;     // UI::ElementDescription
    // ENDGAME B.3.8 block RB (RULE B8-1): the field stores +0x20..+0x67 are
    // ONE body with the real constructor `UI::ElementDescription()` —
    // `InitFields`, in the UI member section of this file.
    UI::ElementDescription::DoorBody::Init(
        reinterpret_cast<UI::ElementDescription*>(self));   // B.3.8 bridge

    return self;   // MOV RAX,RCX — a ctor returns `this`
}

// ?GetFileVersion@ElementDescription@UI@@UEBAHXZ — retail 0x180116af0, ord 2624.
//   MOV EAX,0x4 / RET
// The constant is the .rsc element-description file version and it is what
// ?Serialize@ (held back) is written against.
// ENDGAME B.3.6 fragment pass F2: the body is UI::ElementDescription::
// GetFileVersion (member section below); the door delegates QUALIFIED.
int32_t RtUiElementDescription_GetFileVersion(const ElementDescription* self) {
    return reinterpret_cast<const UI::ElementDescription*>(self)          // B.3.6 bridge
        ->UI::ElementDescription::GetFileVersion();
}

// ============================================================================
// ?TypeName@ElementDescription@UI@@UEBAPEBDXZ  retail 0x1800350f0
// ?Type@ElementDescription@UI@@UEBAIXZ         retail 0x1800350d0
//                                          (WAVE 27 LANE IOTEXT, slots 1 and 2)
// ============================================================================
//
// Two instructions each, both reading one `Object::BaseTypeInfo` at 0x1801faaf8
// -- +0x00 for the type id, +0x08 for the name. The disassembly is quoted
// beside `ElementDescriptionTypeInfo()` above.
//
// NEITHER READS RCX. Retail's `MOV EAX,dword ptr [...]` / `MOV RAX,qword ptr
// [...]` touch no `this`; the parameter is named and unused here for the same
// reason the PackageFile pair (runtime/src/io/package_file.cpp) names it --
// the vtable dispatches them with a this-pointer and the signature must match.
//
// NO ORACLE, AND THE REASON IS NOT "no route" -- IT IS THAT AN ORACLE WOULD BE
// VACUOUS TODAY. `?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A`
// is MINTED (rt_data_mint_tables.inc:1989) but NOT PROMOTED
// (standalone/reg_mint_promoted.cpp carries no row for it), so with the DLL
// present RT_DATA_SLOT falls through to the DLL's own static -- our side and
// retail's side would read the SAME object through the SAME door and a
// comparison would be retail against itself
// (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md).
// What IS checked: the mint shadow compares every minted BaseTypeInfo's fields
// against the loaded DLL's nodes, and that is the check that becomes
// load-bearing the day this row is promoted. Stated, not hidden.
// ENDGAME B.3.6 fragment pass F2: the bodies are UI::ElementDescription::
// TypeName / Type (member section below); the doors delegate QUALIFIED.
// Neither member reads `this`, exactly as retail's two instructions do not
// read RCX.
extern "C" const char* RtUiElementDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::ElementDescription*>(self)          // B.3.6 bridge
        ->UI::ElementDescription::TypeName();
}

extern "C" uint32_t RtUiElementDescription_Type(const void* self) {
    return reinterpret_cast<const UI::ElementDescription*>(self)          // B.3.6 bridge
        ->UI::ElementDescription::Type();
}

// ============================================================================
// ??1ElementDescription@UI@@UEAA@XZ  retail 0x180116100  ord 585
//                                                          (WAVE 9 LANE UIF)
// ============================================================================
//
// WHY THIS ROW EXISTS AT ALL. Like `??1Element`, this destructor is not reached
// by any direct C++ caller in our tree — it is reached through a LIVE MINTED
// VTABLE. `??_7ElementDescription@UI@@6B@` slot 0 is the vector deleting
// destructor `??_EElementDescription@UI@@` at 0x180035350, and lane K's family
// reaches `~Class` by ADDRESS through ResolveAddr.
// With this row unported that edge went to the DLL, and after cutover to NULL.
//
// ⚠ CORRECTION, WAVE 27 lane IOTEXT. This paragraph used to call 0x180035350
// "one of the 37 Wave 9 lane K ported". IT WAS NOT PORTED until Wave 27: at
// wave27/base, grepping 0x180035350 across runtime/, standalone/ and code/
// found it ONLY in this comment and in standalone/rt_data_mint_tables.inc:1016
// (the mint row, i.e. the TRAP), never in a RegisterRuntimeAddr. The sentence
// read as a completion and cost the table three waves of being 3/6
// (reference_a_prescription_read_as_a_completion.md). The row now exists:
// runtime/src/object/deleting_dtor.cpp, spec index 44, "UI::ElementDescription".
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
// `~External` twice over a handle that no longer exists. The did-work split
// how many calls actually released a buffer; `calls=` stays 0 because there is
// no comparison to report. Liveness, not correctness.
void RtUiElementDescription_Dtor(ElementDescription* self) {
    static void* const vtblEd = ResolveExport(RtUI::kVtblElementDescription);
    if (vtblEd) *reinterpret_cast<void**>(self) = vtblEd;

    // ENDGAME B.3.8 block RB (RULE B8-1): the String release between the
    // restamp and the ~External tail is ONE body with the real destructor
    // `UI::ElementDescription::~ElementDescription` — `ReleaseFields`.
    UI::ElementDescription::DoorBody::Release(
        reinterpret_cast<UI::ElementDescription*>(self));   // B.3.8 bridge

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
    // B.3.6: the body is UI::Dialog::CreateElement (bottom of file); RETSLOT
    // door — placement-new the member's prvalue into the caller's slot.
    new (ret) Object::InternalPtr<UI::Element>(
        reinterpret_cast<const UI::Dialog*>(self)           // B.3.6 bridge
            ->UI::Dialog::CreateElement(Object::ExternalPtr<UI::ElementDescription>(
                static_cast<UI::ElementDescription*>(desc))));
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
// The did-work split here is whether the delta was non-zero, so a zero on
// every call means the population never moved a dialog.
void RtUiDialogInstance_SetUpperLeft(DialogInstance* self, int32_t x, int32_t y,
                                     bool scaled) {
    // B.3.6: the body is UI::DialogInstance::SetUpperLeft (bottom of file).
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::SetUpperLeft(x, y, scaled);
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
// (docs/runtime_port/WAVE10_LANE_ALLOC.md): the allocator port runs with the
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

// `?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z` 0x180002470.
using GetIndexFromNameFn = int32_t (*)(const void* table, const void* str);

// The retail-inlined `??0Dialog@UI@@QEAA@XZ` / `??1Dialog@UI@@UEAA@XZ` pair is
// `::new (p) UI::Dialog()` / `DeletingDtor(0)` since ENDGAME B.3.9 UI-D (the
// temporary is a real UI::Dialog wearing the class's table).
constexpr size_t kDialogTempSize = 0x88;

// ENDGAME B.3.6 fragment pass F2: the body that stood here (the file-local
// `EdCreateInstanceBody`) MOVED into UI::ElementDescription::CreateInstance(
// InternalPtr<Element>*) in the member section below; the door delegates
// QUALIFIED.

// (The replay oracle's storage that sat here — g_edCi* — had no reader since
// the oracle was retired; removed with the body move.)

} // namespace

extern "C" void RtUiElementDescription_CreateInstance(const ElementDescription* self,
                                                      void** elemPtr) {
    reinterpret_cast<const UI::ElementDescription*>(self)                 // B.3.6 bridge
        ->UI::ElementDescription::CreateInstance(
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(elemPtr));
}

// ############################################################################
// ENDGAME B.3.6 block B — THE MEMBER BODIES.
//
// Every body that stood in a door above MOVED here onto UI::DialogInstance /
// UI::Dialog / UI::DialogController (runtime/types/ui/{DialogInstance,Dialog,
// DialogController}.h), unchanged except `self->x` became `x` and the
// hand-inlined vtable calls onto THIS block's classes became member calls:
//   * `(*(void***)self)[6/7]`       -> OnUpdateLayout() / OnUpdate(time) (virtual)
//   * controller slot +0x10/+0x38/+0x40/+0x48 -> c->Type() / OnUpdateLayout() /
//     OnUpdate(time) / OnElementEvent(...)  (virtual, on UI::DialogController)
//   * RtUiDialogInstance_Contains/_Update/_GetMouseCursorIndex -> member calls.
// The doors above keep their registrations and delegate QUALIFIED. Calls onto
// OTHER blocks' classes (Element's slots 4/5/7/9, Element::FindElement /
// FindElementType / AdjustUpperLeft, UserInterface::_events) are left as they
// were: those classes are not converted yet.
//
// Per-body evidence and the retail disassembly are in the comment blocks at
// each door above; they are not repeated.
// ############################################################################

namespace {

// Retry-a-miss name doors the member bodies need (RULE B6-5: no member keeps
// a static of its own; the caches live here, in the file-scope helpers).
// NULL when unresolved; the caller keeps the door's `if (getPath)` guard.
using GetPathFn = void* (*)(const void* self, void* out);
inline GetPathFn ExternalGetPathFn() {
    return RT_EXPORT_ONCE(GetPathFn,
        "?GetPath@External@Object@@QEBA?AVFilename@System@@XZ");
}

// ?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z 0x180002470 --
// block D's class, still a name door here. Fragment pass F2 moved the static
// UI::ElementDescription::CreateInstance used to keep out to this helper.
inline GetIndexFromNameFn StringTableGetIndexFromNameFn() {
    return RT_EXPORT_ONCE(GetIndexFromNameFn,
        "?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z");
}

// ?AdjustUpperLeft@Element@UI@@ 0x180115840, reached through the seam
// (ResolveAddr consults the port registry first). Retry-a-miss.
using AdjustFn = void (*)(void*, int32_t, int32_t);
inline AdjustFn ElementAdjustUpperLeft() {
    static AdjustFn adjust = nullptr;
    if (!adjust) adjust = reinterpret_cast<AdjustFn>(
        ResolveAddr(Module::Runtime, 0x180115840));
    return adjust;
}

inline const uint8_t* Bytes(const void* p) { return static_cast<const uint8_t*>(p); }
inline uint8_t*       Bytes(void* p)       { return static_cast<uint8_t*>(p); }

}  // namespace

namespace UI {

// ---- Object::Internal slots ------------------------------------------------
void DialogInstance::Persist(void* /*stream*/) {}              // [3] 0x180114fa0  RET
void DialogInstance::OnSerializeSettings(void* /*stream*/) {}  // [9] 0x180115470  RET

const char* DialogInstance::TypeName() const {                 // [1] 0x180045400
    return *reinterpret_cast<const char* const*>(
        DialogInstanceTypeInfo() + kBaseTypeInfoName);
}

// ?TypeId@DialogInstance@UI@@SAIXZ 0x1800453f0 — the first dword of the
// minted `?_typeInfo@DialogInstance@UI@@` record (ENDGAME B.3.9 UI-D).
uint32_t DialogInstance::TypeId() {
    return *reinterpret_cast<const uint32_t*>(
        DialogInstanceTypeInfo() + kBaseTypeInfoTypeId);
}

uint32_t DialogInstance::Type() const { return TypeId(); }     // [2] 0x1800453e0

// ?CreateNewObject@DialogInstance@UI@@SAPEAVBase@Object@@XZ  0x1800454f0
// The retired FACTUI1 row: the locked MemAlloc of 0xb8 bytes, align 8;
// retail's null branch; then ??0DialogInstance (0x180114ca0) inlined. The
// class exports no CreateInPlaceObject.
Object::Base* DialogInstance::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(DialogInstance), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) DialogInstance();
}

// ============================================================================
// UI::DialogInstance::ReleaseFields — ??1DialogInstance 0x180114d40 between
// the vptr store and the inlined ~Internal (ENDGAME B.3.8 block RB, B8-1).
// The real destructor's body (DialogInstance.h); the door that shared it,
// `RtUiDialogInstance_Dtor`, is retired (ENDGAME B.3.9 UI-D). Moved verbatim
// (`self->x` -> `x`); the four details the old door's banner lists hold.
// ============================================================================
void DialogInstance::ReleaseFields() {
    // --- the root Element, through its scalar deleting destructor ------------
    if (_root) {
        DeletingDtorOf(_root)(_root, 1);
        _root = nullptr;
    }

    // --- every controller, same way. _data is re-read each iteration ---------
    for (int i = 0; i < _controllers._size; ++i) {
        DialogController** data = _controllers._data;   // re-read: see above
        DialogController*  c    = data[i];
        if (c) {
            DeletingDtorOf(c)(c, 1);
            data[i] = nullptr;
        }
    }

    if (_controllers._data) MemFreeStatic(_controllers._data);
    _controllers._data = nullptr;
    _controllers._size = 0;      // _capacity deliberately untouched

    if (_timers._data) MemFreeStatic(_timers._data);
    _timers._data = nullptr;
    _timers._size = 0;           // _capacity deliberately untouched

    _children.Clear();           // retail 0x1800325f0
}


// ---- accessors -------------------------------------------------------------
Object::InternalPtr<Element> DialogInstance::GetRoot() {       // 0x180045580
    return Object::InternalPtr<Element>(static_cast<Element*>(_root));
}

// THE PARAMETER IS A TYPE ID, NOT AN INDEX (see the door). No null check on
// the controller: retail loads [RCX] off it unguarded.
Object::InternalPtr<DialogController>
DialogInstance::GetController(uint32_t typeId) const {         // 0x180113320
    DialogController* found = nullptr;
    for (int32_t i = 0; i < _controllers._size; ++i) {
        DialogController* c = _controllers._data[i];
        if (c->Type() == typeId) {                             // controller slot 2
            found = _controllers._data[i];
            break;
        }
    }
    return Object::InternalPtr<DialogController>(found);
}

// Forwarders into Element (block A's class, still a name door). NO NULL
// CHECK on _root — retail has none.
Object::InternalPtr<Element>
DialogInstance::FindElement(const System::String& name) {      // 0x180114c70
    Object::InternalPtr<Element> out;
    reinterpret_cast<PtrOutStrFn>(ResolveExport(kElementFindElement))(
        _root, reinterpret_cast<void**>(&out), &name);
    return out;
}

Object::InternalPtr<Element>
DialogInstance::FindElementType(uint32_t typeId) {             // 0x1801140c0
    Object::InternalPtr<Element> out;
    reinterpret_cast<PtrOutU32Fn>(ResolveExport(kElementFindElementType))(
        _root, reinterpret_cast<void**>(&out), typeId);
    return out;
}

// ---- self-contained mutators ------------------------------------------------
void DialogInstance::SetFlagsOnOff(uint32_t on, uint32_t off) { // 0x180114690
    _flags = (on | _flags) & ~off;
}

// Transcribed from the DISASSEMBLY (see the door): 64-bit search bound,
// `data + index*0x18` null test on the SUM, one decrement at the end.
void DialogInstance::StopTimer(int32_t id) {                   // 0x180113730
    const int64_t count64 = static_cast<int64_t>(_timers._size);
    if (count64 > 0) {
        uint8_t* base = reinterpret_cast<uint8_t*>(_timers._data);
        int32_t  i = 0;
        int64_t  walked = 0;
        for (;;) {
            if (*reinterpret_cast<const int32_t*>(base + walked * 0x18 + 0x10) == id) break;
            ++walked;
            ++i;
            if (walked >= _timers._size) return;   // retail: plain RET
        }
        if (i < _timers._size) {
            int64_t off = static_cast<int64_t>(i) * 0x18;
            do {
                ++i;
                uint8_t* p = base + off;
                if (i < _timers._size && p != nullptr) {
                    std::memcpy(p + 0x00, p + 0x18, 8);
                    std::memcpy(p + 0x08, p + 0x20, 8);
                    std::memcpy(p + 0x10, p + 0x28, 8);
                }
                off += 0x18;
            } while (i < _timers._size);
        }
        _timers._size -= 1;
    }
}

// ---- the three controller-dispatch virtuals ---------------------------------
// No null check on the controller pointer: retail dereferences it unguarded.
void DialogInstance::OnUpdateLayout() {                        // [6] 0x1801153a0
    for (int32_t i = 0; i < _controllers._size; ++i)
        _controllers._data[i]->OnUpdateLayout();               // controller slot 7
}

void DialogInstance::OnUpdate(const System::Time& time) {      // [7] 0x180115400
    for (int32_t i = 0; i < _controllers._size; ++i)
        _controllers._data[i]->OnUpdate(time);                 // controller slot 8
}

// Retail re-reads `*element` into a fresh stack slot on EVERY iteration and
// hands the ADDRESS of that copy on, so a controller that overwrites its
// argument cannot corrupt the caller's InternalPtr. Kept.
void DialogInstance::OnElementEvent(Object::InternalPtr<Element>* element,
                                    int32_t a, int32_t b, int32_t c4,
                                    int32_t d, int32_t e, void* userData) {  // [8] 0x180114ed0
    for (int32_t i = 0; i < _controllers._size; ++i) {
        DialogController* c = _controllers._data[i];
        Object::InternalPtr<Element> local = *element;
        c->OnElementEvent(&local, a, b, c4, d, e, userData);   // controller slot 9
    }
}

// ---- frame leftovers --------------------------------------------------------
// Element offsets are block A's; spelled raw at the use site (Element.h pins
// them). Element vtable slots: 4 ComputeSize, 5 ComputeLayout, 7 Update,
// 9 Contains.
bool DialogInstance::Contains(int32_t x, int32_t y) const {    // 0x180114820
    const uint8_t* root = Bytes(_root);
    if ((_flags >> 2) & 1) return false;
    if ((_flags >> 3) & 1) {
        void* const* const vtable = *reinterpret_cast<void* const* const*>(root);
        using Fn = bool (*)(const void*, int32_t, int32_t, void*);
        return vtable && reinterpret_cast<Fn>(vtable[9])(root, x, y, _ui);
    }
    const int32_t left   = *reinterpret_cast<const int32_t*>(root + 0x68);
    const int32_t right  = *reinterpret_cast<const int32_t*>(root + 0x70);
    const int32_t top    = *reinterpret_cast<const int32_t*>(root + 0x6c);
    const int32_t bottom = *reinterpret_cast<const int32_t*>(root + 0x74);
    return left <= x && x <= right && top <= y && y <= bottom;
}

int32_t DialogInstance::GetMouseCursorIndex(Object::InternalPtr<Element> element,
                                            int32_t x, int32_t y) const {  // 0x180114010
    const uint8_t* elem = Bytes(static_cast<const void*>(element.Get()));
    int32_t result = *reinterpret_cast<const int32_t*>(elem + 0x3c);
    const int32_t count = *reinterpret_cast<const int32_t*>(elem + 0xa8);
    void* const* const children = *reinterpret_cast<void* const* const*>(elem + 0xa0);
    for (int32_t i = 0; i < count; ++i) {
        void* child = children ? children[i] : nullptr;
        if (!child) continue;
        const uint8_t* c = Bytes(child);
        if ((*reinterpret_cast<const uint8_t*>(c + 0x34) & 3) != 0) continue;
        const int32_t left   = *reinterpret_cast<const int32_t*>(c + 0x68);
        const int32_t top    = *reinterpret_cast<const int32_t*>(c + 0x6c);
        const int32_t right  = *reinterpret_cast<const int32_t*>(c + 0x70);
        const int32_t bottom = *reinterpret_cast<const int32_t*>(c + 0x74);
        if (left <= x && x < right && top <= y && y < bottom)
            result = GetMouseCursorIndex(
                Object::InternalPtr<Element>(static_cast<Element*>(child)), x, y);
    }
    return result;
}

void DialogInstance::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {  // 0x180113d90
    OnUpdateLayout();                                          // own slot 6 — VIRTUAL

    uint8_t* root = Bytes(_root);
    auto ComputeSize = [&]() {
        void** vt = *reinterpret_cast<void***>(root);
        reinterpret_cast<void (*)(uint8_t*)>(vt[4])(root);
    };
    ComputeSize();

    int32_t layoutW = *reinterpret_cast<int32_t*>(root + 0x60);
    int32_t layoutH = *reinterpret_cast<int32_t*>(root + 0x64);
    const int32_t alignment = *reinterpret_cast<int32_t*>(root + 0x30);
    const uint8_t elemFlags = *reinterpret_cast<uint8_t*>(root + 0x34);
    const int32_t leftPad   = *reinterpret_cast<int32_t*>(root + 0x18);
    const int32_t rightPad  = *reinterpret_cast<int32_t*>(root + 0x1c);
    const int32_t topPad    = *reinterpret_cast<int32_t*>(root + 0x20);
    const int32_t bottomPad = *reinterpret_cast<int32_t*>(root + 0x24);

    int32_t posX = _upperLeftX + x;
    int32_t posY = _upperLeftY + y;
    if (w < posX + layoutW) posX = w - layoutW;
    if (h < posY + layoutH) posY = h - layoutH;

    const int32_t availRight  = w - rightPad;
    const int32_t availBottom = h - bottomPad;
    posX += leftPad;
    posY += topPad;
    int32_t availW = availRight - posX;
    int32_t availH = availBottom - posY;
    if ((elemFlags >> 2) & 1) layoutW = availW;
    if ((elemFlags >> 3) & 1) layoutH = availH;

    switch (alignment) {
        case 1: case 4: case 7: posX = posX + (availW >> 1) - (layoutW >> 1); break;
        case 2: case 5: case 8: posX = availRight - layoutW; break;
        default: break;   // left-aligned: posX unchanged
    }
    if (alignment > 2) {
        if (alignment < 6)      posY = posY + (availH >> 1) - (layoutH >> 1);
        else if (alignment < 9) posY = availBottom - layoutH;
    }

    ComputeSize();   // retail calls it a second time; reproduced as-is.
    int32_t totalW = *reinterpret_cast<int32_t*>(root + 0x60) + rightPad + leftPad;
    int32_t totalH = *reinterpret_cast<int32_t*>(root + 0x64) + bottomPad + topPad;
    const int32_t minW = *reinterpret_cast<int32_t*>(root + 0x28);
    const int32_t minH = *reinterpret_cast<int32_t*>(root + 0x2c);
    if (totalW < minW) totalW = minW;
    if (totalH < minH) totalH = minH;

    const uint32_t rootFlags = *reinterpret_cast<uint32_t*>(root + 0x34);
    if (rootFlags & 0x30) {
        uint8_t* ancestor = root;
        while (*reinterpret_cast<uint8_t**>(ancestor + 0x98) != nullptr)
            ancestor = *reinterpret_cast<uint8_t**>(ancestor + 0x98);
        // ancestor+0x90 is Element::_dialogParent; its +0x10 is
        // DialogInstance::_ui, whose +0xdc is UserInterface::_uiScale.
        uint8_t* dlgParent = *reinterpret_cast<uint8_t**>(ancestor + 0x90);
        const float uiScale = dlgParent
            ? *reinterpret_cast<float*>(*reinterpret_cast<uint8_t**>(dlgParent + 0x10) + 0xdc)
            : 1.0f;
        void** deviceSlot = VideoDeviceSingleton();
        const uint8_t* device = deviceSlot ? static_cast<uint8_t*>(*deviceSlot) : nullptr;
        if (device) {
            if (rootFlags & 0x20)
                totalW = static_cast<int32_t>(static_cast<float>(*reinterpret_cast<const int32_t*>(device + 4)) * uiScale);
            if (rootFlags & 0x10)
                totalH = static_cast<int32_t>(static_cast<float>(*reinterpret_cast<const int32_t*>(device + 8)) * uiScale);
        }
    }

    void** vt = *reinterpret_cast<void***>(root);
    reinterpret_cast<void (*)(uint8_t*, int32_t, int32_t, int32_t, int32_t)>(vt[5])(
        root, posX, posY, posX + totalW, posY + totalH);
}

void DialogInstance::Update(const System::Time& time, UserInterface* ui,
                            uint8_t* updateState) {            // 0x180113980
    // The 0x30-byte UI event record (see the door's banner).
    struct EventRecord {
        void*    dialog;
        uint64_t _unused08;
        int32_t  type;
        int32_t  data;
        uint64_t payload;
        uint64_t _unused20_28[2];
    };
    static_assert(sizeof(EventRecord) == 0x30, "UI::UserInterface::Event stride");
    // UserInterface::_events (+0x48) through the hand-derived kSpec_Event30
    // row, which leaves bytes [0x24,0x28) of every element UNWRITTEN — see
    // the door's banner for why the trait-derived member is not used.
    auto PushEvent = [&](int32_t type, int32_t data, uint64_t payload) {
        EventRecord rec{};
        rec.dialog = this;
        rec.type = type;
        rec.data = data;
        rec.payload = payload;
        uint8_t* eventsHdr = reinterpret_cast<uint8_t*>(ui) + 0x48;  // UserInterface::_events
        auto* events = rtcoll::AsArray<&rtcoll::kSpec_Event30>(eventsHdr);
        events->SetSizeFill(events->GetSize() + 1, &rec);          // retail 0x180033690
    };

    bool anyChildBit1 = false;
    for (auto it = _children.GetFirst(); it; ++it) {
        DialogInstance* child = it->Get();
        anyChildBit1 = anyChildBit1 || ((child->_flags >> 1) & 1) != 0;
    }

    if (!anyChildBit1) {
        OnUpdate(time);                                        // own slot 7 — VIRTUAL

        const uint8_t* us = updateState;
        const int64_t timeTicks = *reinterpret_cast<const int64_t*>(&time);  // System::Time: one int64
        const double secondsPerTick = *RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
        // DAT_1801616b8: a double, read directly out of the shipped DLL
        // (0.25 seconds) — no export names it, so it is a literal here.
        constexpr double kTooltipDelaySeconds = 0.25;

        if (us[8] == 0 && us[9] == 0 && us[7] == 0) {
            const int32_t mx = *reinterpret_cast<const int32_t*>(us + 0x18);
            const int32_t my = *reinterpret_cast<const int32_t*>(us + 0x1c);
            if (us[0x20] == 0 && Contains(mx, my)) {
                // +0x90/+0x94, NOT the upper-left pair at +0x88 (see the door).
                if (_hoverMouseX == mx && _hoverMouseY == my) {
                    _hoverTime += timeTicks;
                    if (_hoverTime > static_cast<int64_t>(kTooltipDelaySeconds / secondsPerTick))
                        _hovering = 1;
                }
                _hoverMouseX = mx;
                _hoverMouseY = my;
            }
        } else if (_hovering != 0) {
            _hovering = 0;
            PushEvent(0x11, 0, 0);
            _hoverTime = 0;
        }

        // ---- timers: +0x08 is the LIVE countdown (see the door) -----------
        if (_timers._size > 0) {
            uint8_t* data = reinterpret_cast<uint8_t*>(_timers._data);
            int32_t i = 0;
            while (i < _timers._size) {
                uint8_t* t = data + static_cast<size_t>(i) * 0x18;
                int64_t* remaining = reinterpret_cast<int64_t*>(t + 0x08);
                *remaining -= timeTicks;
                if (*remaining <= 0) {
                    const int32_t timerId = *reinterpret_cast<int32_t*>(t + 0x10);
                    PushEvent(0x13, timerId, 0);
                    for (int32_t k = i; k + 1 < _timers._size; ++k)
                        std::memcpy(data + static_cast<size_t>(k) * 0x18,
                                    data + static_cast<size_t>(k + 1) * 0x18, 0x18);
                    --_timers._size;
                    continue;   // retail does NOT re-visit the slot just filled
                }
                ++i;
            }
        }

        // ---- root Element::Update (block A's slot 7, five arguments) -------
        uint8_t* root = Bytes(_root);
        DialogInstance* topDialog = this;
        {
            void** vt = *reinterpret_cast<void***>(root);
            reinterpret_cast<void (*)(uint8_t*, const void*, void*, DialogInstance**, uint8_t*)>(
                vt[7])(root, &time, ui, &topDialog, updateState);
        }

        const int32_t mx2 = *reinterpret_cast<const int32_t*>(updateState + 0x18);
        const int32_t my2 = *reinterpret_cast<const int32_t*>(updateState + 0x1c);
        // bit2 is mouse-passthrough: it SKIPS the hit block (see the door).
        bool hit;
        if ((_flags >> 2) & 1) {
            hit = false;
        } else if ((_flags >> 3) & 1) {
            void** vt = *reinterpret_cast<void***>(root);
            using Fn = bool (*)(uint8_t*, int32_t, int32_t, const void*);
            hit = reinterpret_cast<Fn>(vt[9])(root, mx2, my2, _dialog);
        } else {
            const int32_t left   = *reinterpret_cast<const int32_t*>(root + 0x68);
            const int32_t right  = *reinterpret_cast<const int32_t*>(root + 0x70);
            const int32_t top    = *reinterpret_cast<const int32_t*>(root + 0x6c);
            const int32_t bottom = *reinterpret_cast<const int32_t*>(root + 0x74);
            hit = left <= mx2 && mx2 <= right && top <= my2 && my2 <= bottom;
        }
        if (hit) {
            const uint64_t mousePos = *reinterpret_cast<const uint64_t*>(updateState + 0x18);
            if (updateState[1] != 0) PushEvent(8,  0, mousePos);
            if (updateState[3] != 0) PushEvent(9,  0, mousePos);
            if (updateState[2] != 0) PushEvent(0xa, 0, mousePos);
            *reinterpret_cast<uint16_t*>(updateState + 7) = 0;
            *reinterpret_cast<uint16_t*>(updateState + 1) = 0;
            *reinterpret_cast<uint16_t*>(updateState + 4) = 0;
            updateState[9] = 0;
            updateState[3] = 0;
            updateState[6] = 0;
            updateState[0x20] = 1;
        }
    }

    for (auto it = _children.GetFirst(); it; ++it)
        it->Get()->Update(time, ui, updateState);
}

void DialogInstance::SetUpperLeft(int32_t x, int32_t y, bool scaled) {  // 0x180114a60
    const float scale = *reinterpret_cast<const float*>(Bytes(_ui) + 0xdc);

    const uint8_t* device = static_cast<const uint8_t*>(*VideoDeviceSingleton());
    const int32_t screenW = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<const int32_t*>(device + 4)) * scale);
    const int32_t screenH = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<const int32_t*>(device + 8)) * scale);

    if (scaled) {
        x = static_cast<int32_t>(static_cast<float>(x) * scale);
        y = static_cast<int32_t>(static_cast<float>(y) * scale);
    }

    uint8_t* root = Bytes(_root);

    // Element vtable slot 4 — ComputeSize. Pure virtual on the base.
    {
        void** vtbl = *reinterpret_cast<void***>(root);
        reinterpret_cast<void (*)(void*)>(vtbl[4])(root);
    }

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

    // THE CLAMP ORDER IS LOAD-BEARING (see the door).
    if (x < 0) x = 0;
    if (w + x > screenW) x = screenW - w;
    if (y < 0) y = 0;
    if (h + y > screenH) y = screenH - h;

    const int32_t dx = x - _upperLeftX;
    const int32_t dy = y - _upperLeftY;

    if (dx != 0 || dy != 0) {
        uint8_t* r = Bytes(_root);
        *reinterpret_cast<int32_t*>(r + 0x68) += dx;
        *reinterpret_cast<int32_t*>(r + 0x70) += dx;
        *reinterpret_cast<int32_t*>(r + 0x6c) += dy;
        *reinterpret_cast<int32_t*>(r + 0x74) += dy;

        const AdjustFn adjust = ElementAdjustUpperLeft();
        const int32_t n = *reinterpret_cast<const int32_t*>(r + 0xa8);
        for (int32_t i = 0; i < n; ++i) {
            void* child = (*reinterpret_cast<void** const*>(r + 0xa0))[i];
            if (child && adjust) adjust(child, dx, dy);
        }
        if (void* border = *reinterpret_cast<void* const*>(r + 0xb0))
            if (adjust) adjust(border, dx, dy);

        _upperLeftX = x;
        _upperLeftY = y;
    }
}

// ============================================================================
// UI::Dialog
// ============================================================================

// Six instructions, no bounds check; MOVSXD, so a negative index reads BEFORE
// the array, faithfully.
Object::ExternalPtr<ElementDescription>
Dialog::GetTemplate(int32_t index) const {                     // 0x1800388b0
    return Object::ExternalPtr<ElementDescription>(
        static_cast<ElementDescription*>(_templates[static_cast<int64_t>(index)]));
}

// "Make an element from this description, unless a NAMED TEMPLATE overrides
// it." Full evidence (the inline strcmp, the slot-8 dispatch, the release
// order) at the door above.
Object::InternalPtr<Element>
Dialog::CreateElement(Object::ExternalPtr<ElementDescription> description) const {  // 0x180112ed0
    void* const desc = description.Get();
    Object::InternalPtr<Element> result;
    if (!desc) return result;

    char* name = nullptr;
    if (const GetPathFn getPath = ExternalGetPathFn()) {
        // identTmp's destructor runs before pathTmp's — retail's order.
        System::Filename pathTmp;
        getPath(desc, &pathTmp);
        const System::Filename identTmp = pathTmp.GetPathIdentifer();
        name = identTmp._data;
        if (name) { ++RtSystem::StringRefCount(name); }   // 0x180112f3e INC [RCX-4]
    }

    void* target  = desc;          // the no-match answer: the argument itself
    bool  matched = false;
    const int32_t count = _namedTemplateCount;
    for (int32_t i = 0; i < count; ++i) {
        const RtUI::DialogNamedTemplate& e = _namedTemplates[i];
        const int32_t lenOurs   = name    ? *reinterpret_cast<const int32_t*>(name    - 8) : 0;
        const int32_t lenTheirs = e._name ? *reinterpret_cast<const int32_t*>(e._name - 8) : 0;
        if (lenOurs != lenTheirs) continue;

        if (!name)             { matched = true; }            // 0x180112fd5
        else if (lenOurs == 0) { matched = true; }            // 0x180112fdb
        else if (!e._name)     { matched = false; }           // LEA ECX,[R8+1] -> 1
        else {
            // The inlined strcmp at 0x180112fe8..0x180113021, transcribed.
            const char* a = name;
            const char* b = e._name;
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
        }
        if (matched) { target = e._description; break; }
    }

    if (matched && !target) {
        // 0x180113045: matched but a null description — 0, no dispatch.
        result = Object::InternalPtr<Element>();
    } else {
        // The description's slot 8 (CreateInstance, RETSLOT, B6-2) — a
        // virtual on UI::ElementDescription, dispatched through the leaf's
        // own table (fragment pass F2: was a hand-inlined vtbl[8]).
        static_cast<const ElementDescription*>(target)->CreateInstance(&result, this);
    }

    if (name) ReleaseStringLocal(name);   // 0x180113061..0x180113087
    return result;
}

// ============================================================================
// UI::DialogController
// ============================================================================
Object::InternalPtr<Element> DialogController::GetContainer() {  // 0x18003b700
    return Object::InternalPtr<Element>(static_cast<Element*>(_container));
}

// ============================================================================
// UI::ElementDescription — ENDGAME B.3.6 fragment pass F2
// ============================================================================
// Block A declared these on the class and left them undefined because their
// bodies were registered doors in THIS file. They moved here unchanged except
// `self->x` / `d + 0x..` became member access; the doors above delegate
// QUALIFIED. Per-body evidence (the retail disassembly) stays at each door.

const char* ElementDescription::TypeName() const {                 // [1] 0x1800350f0
    return *reinterpret_cast<const char* const*>(
        ElementDescriptionTypeInfo() + kBaseTypeInfoName);
}

uint32_t ElementDescription::Type() const {                        // [2] 0x1800350d0
    return *reinterpret_cast<const uint32_t*>(
        ElementDescriptionTypeInfo() + kBaseTypeInfoTypeId);
}

// `MOV EAX,0x4 / RET` — the .rsc element-description file version.
int32_t ElementDescription::GetFileVersion() const {               // [3] 0x180116af0
    return 4;
}

// ??0ElementDescription@UI@@QEAA@XZ 0x180115a40 MINUS the inlined External
// ctor and the three vptr stores (ENDGAME B.3.8 block RB, RULE B8-1). Shared by
// `ElementDescription()` (ElementDescription.h) and the door
// `RtUiElementDescription_Ctor`. Retail writes these as QWORD pairs; the bytes
// are identical.
void ElementDescription::InitFields() {
    _x = 0;              _y = 0;              // +0x20
    _width = 0;          _height = 0;         // +0x28
    _minWidth = 0;       _minHeight = 0;      // +0x30
    _tabOrder = 0;                            // +0x38
    _id = -1;                                 // +0x3c
    _elementType = 3;    _flags = 0;          // +0x40 (QWORD 3)
    _childDesc = nullptr;                     // +0x48
    _userData = 0;                            // +0x50
    _stringTable = nullptr;                   // +0x58
    _textName = nullptr;                      // +0x60
}

// ??1ElementDescription@UI@@UEAA@XZ 0x180116100 MINUS the restamp and the
// ~External tail: the one String release (ENDGAME B.3.8 block RB). Shared by
// `~ElementDescription()` and the door `RtUiElementDescription_Dtor`; moved
// verbatim (the door's never-read `didWork` flag is gone).
void ElementDescription::ReleaseFields() {
    // Retail's String release, with its asymmetry intact: the FREEING branch
    // leaves the field holding `data - 8` (dangling), the SHARING branch nulls
    // it. See runtime/types/system/String.h.
    if (char* p = _textName) {
        if (--RtSystem::StringRefCount(p) == 0) {
            _textName = p - 8;
            using FreeFn = void (*)(void*);
            static FreeFn freeFn = reinterpret_cast<FreeFn>(
                ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
            if (freeFn) freeFn(_textName);
        } else {
            _textName = nullptr;
        }
    }
}


// ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z
// 0x180115ad0. RULE B6-11: the handle arrives by HIDDEN POINTER (retail
// dereferences RDX eleven times), so the member takes `InternalPtr<Element>*`.
// The full evidence block (facts (a)..(c)) is at the door above.
void ElementDescription::CreateInstance(Object::InternalPtr<Element>* element) const {
    // ---- 1. elem->_name = this->GetPath().GetPathIdentifer() --------------
    // Object::External::GetPath stays a name lookup (another batch's class);
    // GetPathIdentifer is System::Filename's member.
    const GetPathFn getPath = ExternalGetPathFn();

    uint8_t* elem = reinterpret_cast<uint8_t*>(element->Get());

    // The inner scope ends the two temporaries' lives where retail's two
    // inlined releases are, in retail's order (identTmp, then pathTmp).
    {
        System::Filename pathTmp;
        System::Filename identTmp;
        if (getPath) {
            getPath(this, &pathTmp);
            identTmp = pathTmp.GetPathIdentifer();
        }

        char*    want = identTmp._data;
        // B.3.1 bridge: Element::_name at +0x10, retail's inlined assign stays.
        char*    old  = *reinterpret_cast<char**>(elem + 0x10);
        if (old != want) {
            if (old) {
                if (--RtSystem::StringRefCount(old) == 0) {
                    // Retail's asymmetry: the FREEING branch leaves the FIELD
                    // holding `data - 8` and frees that; sharing nulls it.
                    *reinterpret_cast<char**>(elem + 0x10) = old - 8;
                    MemFreeStatic(*reinterpret_cast<char**>(elem + 0x10));
                } else {
                    *reinterpret_cast<char**>(elem + 0x10) = nullptr;
                }
            }
            *reinterpret_cast<char**>(elem + 0x10) = want;
            if (want) ++RtSystem::StringRefCount(want);
        }
    }

    // ---- 2. the eleven scalar copies --------------------------------------
    // The handle is RE-READ before every store, exactly as retail does. Nothing
    // in between can change it, but the re-read is what the shipped bytes do
    // and transcribing it costs nothing.
    #define ED_ELEM  (reinterpret_cast<uint8_t*>(element->Get()))
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x30) = static_cast<int32_t>(_elementType); // -> _alignment
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x18) = _x;
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x1c) = _y;
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x20) = _width;
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x24) = _height;
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x28) = _minWidth;
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x2c) = _minHeight;
    *reinterpret_cast<uint32_t*>(ED_ELEM + 0x34) |= _flags;      // OR, never assign
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x38) = _id;           // _id      -> +0x38
    *reinterpret_cast<int32_t*>(ED_ELEM + 0x3c) = _tabOrder;     // _tabOrder-> +0x3c
    *reinterpret_cast<uint64_t*>(ED_ELEM + 0x40) = _userData;

    // ---- 3. the tool-tip UIString -----------------------------------------
    void*   table = _stringTable;
    int32_t index = -1;
    if (table) {
        if (const GetIndexFromNameFn gifn = StringTableGetIndexFromNameFn())
            index = gifn(table, &_textName);
    }
    elem = ED_ELEM;
    *reinterpret_cast<void**>(elem + 0x48)  = table;
    // Fact (b): retail writes a QWORD here whose upper half is uninitialised
    // stack. The port writes the DWORD only.
    *reinterpret_cast<int32_t*>(elem + 0x50) = index;

    // ---- 4. elem->_description = this -------------------------------------
    *reinterpret_cast<const void**>(ED_ELEM + 0xb8) = this;

    // ---- 5. the UI::Dialog temporary and the one virtual call -------------
    alignas(16) uint8_t tempDialog[kDialogTempSize];
    UI::Dialog* const tempDlg = ::new (static_cast<void*>(tempDialog)) UI::Dialog();

    Object::InternalPtr<Element>  slot;
    Object::InternalPtr<Element>* ret = &slot;
    if (_childDesc != nullptr) {
        // The child description's slot 8 (CreateInstance, RETSLOT, B6-2) — a
        // virtual on this class, dispatched through the child's own table
        // (was a hand-inlined vtable +0x40 call).
        ret = static_cast<const ElementDescription*>(_childDesc)->CreateInstance(
            &slot, tempDlg);
    } else {
        slot = Object::InternalPtr<Element>();
        ret  = &slot;
    }
    *reinterpret_cast<void**>(ED_ELEM + 0xb0) = ret->Get();

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
    tempDlg->DeletingDtor(0);         // ~Dialog: see fact (a)
}

}  // namespace UI

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

    // ---- UI leftovers batch (docs/runtime_port/closure/WORKLIST.md) --------
    RegisterRuntimeExport("?Contains@DialogInstance@UI@@QEBA_NHH@Z",
                          (void*)&RtUiDialogInstance_Contains);
    RegisterRuntimeAddr(0x180114820, (void*)&RtUiDialogInstance_Contains);            ++n;

    RegisterRuntimeExport(
        "?GetMouseCursorIndex@DialogInstance@UI@@QEBAHV?$InternalPtr@VElement@UI@@@Object@@HH@Z",
        (void*)&RtUiDialogInstance_GetMouseCursorIndex);
    RegisterRuntimeAddr(0x180114010, (void*)&RtUiDialogInstance_GetMouseCursorIndex); ++n;

    RegisterRuntimeExport("?ComputeLayout@DialogInstance@UI@@QEAAXHHHH@Z",
                          (void*)&RtUiDialogInstance_ComputeLayout);
    RegisterRuntimeAddr(0x180113d90, (void*)&RtUiDialogInstance_ComputeLayout);       ++n;

    RegisterRuntimeExport(
        "?Update@DialogInstance@UI@@QEAAXAEBVTime@System@@PEAVUserInterface@2@AEAUUpdateState@Element@2@@Z",
        (void*)&RtUiDialogInstance_Update);
    RegisterRuntimeAddr(0x180113980, (void*)&RtUiDialogInstance_Update);              ++n;

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

    // WAVE 27 lane IOTEXT -- `??_7ElementDescription@UI@@6B@` slots 2 and 1.
    // BY NAME *AND* BY ADDRESS, because the minted table fills a NAMED row
    // through the name and an in-tree caller can arrive by address; a name-only
    // registration is invisible to the second door and dies at cutover
    // (reference_count_the_doors_not_the_calls.md).
    RegisterRuntimeExport("?Type@ElementDescription@UI@@UEBAIXZ",
                          (void*)&RtUiElementDescription_Type);
    RegisterRuntimeAddr(0x1800350d0, (void*)&RtUiElementDescription_Type);            ++n;

    RegisterRuntimeExport("?TypeName@ElementDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUiElementDescription_TypeName);
    RegisterRuntimeAddr(0x1800350f0, (void*)&RtUiElementDescription_TypeName);        ++n;

    // (ENDGAME B.3.9 UI-D: the DialogInstance slot doors -- Persist,
    // OnSerializeSettings, TypeName, Type -- and the ??1DialogInstance door
    // went with the minted table.) The static export and the factory are the
    // class's own; both doors on each.
    RegisterRuntimeExport("?TypeId@DialogInstance@UI@@SAIXZ",
                          (void*)&UI::DialogInstance::TypeId);
    RegisterRuntimeAddr(0x1800453f0, (void*)&UI::DialogInstance::TypeId);              ++n;
    RegisterRuntimeExport("?CreateNewObject@DialogInstance@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::DialogInstance::CreateNewObject);
    RegisterRuntimeAddr(0x1800454f0, (void*)&UI::DialogInstance::CreateNewObject);     ++n;

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

    return n;
}
