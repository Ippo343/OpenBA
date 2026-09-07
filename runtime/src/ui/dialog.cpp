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
//   | ?StartTimer@                          | 0x1801137c0 | 4081 | NO     | —      |
//   | ?StopTimer@                           | 0x180113730 | 4111 | yes    | replay |
//   | ?CreateElement@Dialog@                | 0x180112ed0 | 1888 | NO     | —      |
//   | ?GetReference@Dialog@ (as spelled)    |  NOT AN EXPORT     | n/a    | —      |
//   | ?GetTemplate@Dialog@                  | 0x1800388b0 | 2956 | yes    | replay |
//   | ?_typeInfo@Dialog@ (data)             | 0x1801faa88 | 4847 | n/a    | —      |
//   | ??1DialogController (dtor)            | 0x1800cd2f0 |  572 | yes    | shape  |
//   | ?GetContainer@DialogController@       | 0x18003b700 | 2508 | yes    | replay |
//   | ??0ElementDescription (ctor)          | 0x180115a40 |  173 | yes    | replay |
//   | ??1ElementDescription (dtor)          | 0x180116100 |  585 | NO     | —      |
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

}  // namespace

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
                        ResolveExportRaw("??_7DialogController@UI@@6B@"),
                        ResolveExportRaw("??1Internal@Object@@UEAA@XZ"));
    }

    return n;
}
