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
//   | ??1DialogInstance (dtor)              | 0x180114d40 |  574 | NO     | —      |
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
//   | ?OnClosed@DialogInstance@             | 0x180115330 | 3269 | NO     | —      |
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
//   | ?CreateInstance@ElementDescription@   | 0x180115ad0 | 2004 | NO     | —      |
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
//   1. NOTHING PORTED HERE ALLOCATES, FREES, OR RESHAPES A TREE. Every ported
//      body either reads a field, writes fields of an object somebody else
//      owns at offsets static_assert'ed in the headers, or walks an existing
//      array. Every function that allocates (both AddControllers, StartTimer,
//      CreateElement, CreateInstance, the two ctors) or frees (both dtors,
//      ~ElementDescription) is held back — see §2 of WAVE7_LANE_B.md.
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
#include "../../types/ui/Dialog.h"
#include "../../types/ui/DialogInstance.h"
#include "../../types/ui/ElementDescription.h"
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

// ---- ?OnClosed@DialogInstance@UI@@UEAAXXZ  0x180115330  ord 3269 — DECLINED
//
// The fourth virtual that site asks for, and it is NOT ported, because porting
// it would MOVE the cutover row rather than close it (WAVE9.md, "porting a
// function can GROW the cutover surface"). Its body is:
//
//     18011533d  CALL 0x180114240        ; ?SaveSettings@DialogInstance@UI@@QEAAXXZ
//     ... then walk _controllers (+0x78, count +0x80) calling each
//         controller's vtable slot +0x30 ...
//
// `SaveSettings` is 507 bytes, is itself exported (ordinal 3613) and is NOT
// ported by any wave. A port of `OnClosed` would therefore resolve one name at
// cutover and immediately request another that resolves to null — the same null
// call, one frame deeper and under a different name. It becomes a one-function
// job the moment `?SaveSettings@` lands.
//
// (The controller loop's slot +0x30 is a fifth `DialogController` vtable slot
// not yet in runtime/types/ui/DialogInstance.h's enum, which lists +0x10, +0x38,
// +0x40 and +0x48. Recorded here rather than added to that header, because a
// slot pinned by ONE unported caller is not yet evidence of a name.)

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
    }

    return n;
}
