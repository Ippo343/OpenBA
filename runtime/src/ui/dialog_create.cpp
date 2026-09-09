// ============================================================================
// runtime/src/ui/dialog_create.cpp — Wave 10 lane UI3
//   The four allocating UI::DialogInstance rows Wave 7 lane B held back.
// ============================================================================
//
//   | Export (short form)                    | Retail      | Ord  |
//   |----------------------------------------|-------------|------|
//   | ??0DialogInstance (ctor)               | 0x180114870 |  147 |
//   | ?AddController@ (…ExtPtr<ED>, String&) | 0x180113470 | 1541 |
//   | ?AddController@ (…IntPtr<Element>)     | 0x1801133b0 | 1542 |
//   | ?StartTimer@                           | 0x1801137c0 | 4081 |
//
// All four mangled names were read back out of the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with tools/pe_exports.py BEFORE a line
// of body was written, and all four are present at exactly those addresses
// (reference_resolveexport_phantom_names.md — a name the DLL does not contain
// resolves to NULL in silence and the census still counts the registration).
// The full query output is in docs/runtime_port/WAVE10_LANE_UI3.md §1.
//
// ---------------------------------------------------------------------------
// THE BLOCKER THESE FOUR SAT BEHIND, AND WHY IT IS ANSWERED
// ---------------------------------------------------------------------------
// WAVE7_LANE_B.md §2a held all four back under one heading: "eight allocate or
// free — split ownership, and no oracle can be built". Three sub-claims, each
// re-derived here rather than inherited
// (reference_holdback_blockers_are_stale_check_them_first.md):
//
//  1. "`EnterCriticalSection` is Win32 and cannot appear in a non-`_win` file."
//     TRUE OF THE INLINED FORM, and irrelevant. Every allocation these four
//     make is the byte-for-byte body of an EXPORTED static:
//         ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z   0x1800c7a50
//             = Enter(_singleton+0x640); Allocate(_singleton,n,align); Leave
//         ?MemFree@MemoryAllocator@System@@SAXPEAX@Z      0x1800c7ad0
//     runtime/src/collection/array.cpp made exactly this call three waves ago
//     and states the argument: "calling it IS retail's body minus the inlining,
//     and it keeps <windows.h> out of runtime/src/". This file follows it and
//     uses the SAME two accessors, by address, for the same reason.
//
//  2. "The node it allocates is freed by `??1DialogInstance`, which stays in
//     the DLL." BOTH HALVES ARE NOW FALSE. `??1DialogInstance@UI@@` (0x180114d40)
//     was ported by Wave 9 lane UID and is registered in dialog.cpp. And even
//     when the DLL's own dtor runs (it still does, through the DLL's own direct
//     near-CALLs, which no registry can redirect), it frees into the SAME heap:
//     `MemAlloc`/`MemFree` reach 0x1800c7a50/0x1800c7ad0, which is the DLL's
//     allocator when `BANISHED_RT_ALLOC` is unset and OUR ported allocator when
//     it is armed — the Wave 10 lane ALLOC swap patches the DLL's own
//     `Allocate`/`Free` prologues, so the DLL's internal frees follow us too.
//     THERE IS NO CONFIGURATION IN WHICH THE TWO SIDES SEE DIFFERENT HEAPS.
//     That is the whole of the "split ownership" objection and it does not
//     survive either setting of the flag.
//
//  3. "`MemFree` is Wave 2's allocator port, which shipped opt-in with a NO-GO."
//     EXPIRED. docs/runtime_port/WAVE10_LANE_ALLOC.md returns GO: armed with
//     the DLL present, 901,685 allocations and 540,750 frees served by the
//     ported bodies in 30 s, a 3,000-frame redzone stress town with ZERO
//     violations, and an unarmed run byte-identical in behaviour. It remains
//     OPT-IN, which is why point 2 above is stated for BOTH settings.
//
// >>> WHICH CONCLUSIONS DEPEND ON THE ARMED CONFIGURATION: NONE OF THEM. <<<
// Nothing in this file reads `BANISHED_RT_ALLOC`, and the allocator door it
// uses is an address that resolves to whichever allocator is live. The lane
// ran its verification under BOTH settings and the doc records both.
//
// ---------------------------------------------------------------------------
// THE FOURTH CLAIM — "no oracle can be built" — AND WHAT REPLACED IT
// ---------------------------------------------------------------------------
// A replay oracle is genuinely impossible for all four: running the body twice
// appends two timers, constructs two controllers, creates two Element trees.
// That is not the same as "no oracle".
//
//  (a) SYNTHETIC-SUBJECT DIFFERENTIAL — `?StartTimer@`.
//      Two private, identical subjects; OURS runs on one and THE SHIPPED BODY
//      runs on the other; the resulting `_timers` headers and every element are
//      compared. The DLL is the authority, it runs for real, and nothing live
//      is touched. This is array_probe.cpp's instrument applied to a
//      DialogInstance-shaped subject, and it drives a case matrix a live boot
//      never produces (empty array, first append, capacity growth, id hit,
//      retail's deliberate duplicate append). It carries a NEGATIVE CONTROL
//      that must be caught (reference_probe_needs_an_effectiveness_control.md).
//
//  (b) SHIPPED-BYTE DISPLACEMENT PINS — all four.
//      Every structure offset, every immediate and every callee address these
//      bodies use is a literal in the shipped instruction stream. `PinDisp`
//      LOCATES each one BY PATTERN (never by a hardcoded byte offset — that is
//      exactly the input that made uic_bytediff.py's first version report a
//      confident wrong mismatch), requires the pattern to occur EXACTLY ONCE,
//      and compares the decoded value against the constant this port actually
//      compiles in. The authority is the DLL, not our expectation, so this is
//      NOT the RT_DIFF_SHADOW self-check family
//      (reference_shadow_marker_is_not_an_oracle.md). It cannot catch a
//      control-flow error; it catches the entire transcription surface, which
//      is where this family of defect actually lives. It runs once at
//      registration and reports through ordinary RtDiff sites, so a `calls=1`
//      row is one CHECK, not a barely-exercised function — the same note
//      WAVE7_LANE_B.md §4 attaches to its shape checks.
//      IT TOO CARRIES A NEGATIVE CONTROL: `PinDisp` is re-run against a
//      deliberately wrong expected value and must reject.
//
//  (c) LIVE POST-STATE DIFFERENTIAL — the two AddControllers and the ctor.
//      See `RtUiDialogCreateProbe` at the bottom. The DLL's own body is run on
//      a private subject built from arguments HARVESTED at a real call, and its
//      result is compared field-by-field against ours.
//
// WHAT IS *NOT* COVERED, said out loud so a green board is not over-read:
//   * The ctor's `dialog->_rootDescription->vtbl[8]` call and the 6-arg's
//     `desc->vtbl[8]` call build whole Element trees inside the DLL. Their
//     writes are not modelled by anything here; only the pointer they hand back
//     and what we do with it are.
//   * The 6-arg's `??0Dialog@UI@@` / `??1Dialog@UI@@` temporary is verified as
//     BEING those two exported bodies (the inlined sequence was diffed against
//     them instruction for instruction, §3 of the lane doc), not re-derived.
//
// ---------------------------------------------------------------------------
// TWO UNEXPORTED ARRAY HELPERS, AND WHY REGISTERING THEM IS NOT NEW WORK
// ---------------------------------------------------------------------------
// `?StartTimer@` tail-calls 0x180076250 and the 4-arg `?AddController@`
// tail-calls 0x180076370. Neither is an export. Neither needed porting:
//
//   0x180076250  ≡  0x18005fb00   (registered, `kSpec_Node18`, stride 0x18)
//   0x180076370  ≡  0x180076900   (registered, `kSpec_Fill8Z`, stride 8)
//
// "≡" is a MEASUREMENT, not an impression: both pairs were compared
// instruction-for-instruction with the leading address stripped and every
// INTERNAL branch target rewritten as a body-relative delta, so every operand
// naming anything outside the body (0x1800c7a50, 0x1801fc718, 0x18014b390,
// 0x1800c6c00, 0x18014b388) had to match ABSOLUTELY
// (reference_position_only_diffs_hide_one_body.md). Both pairs: 0 differing
// instructions. THE INSTRUMENT WAS CONTROLLED — the same comparison between
// the stride-8 body and the stride-0x18 body reports 62 differing instructions,
// so it is not structurally incapable of saying "different"
// (reference_diagnostic_field_cannot_express_the_answer.md). The two extra
// `RegisterRuntimeAddr` lines live in runtime/src/collection/array.cpp beside
// the bodies they name, NOT here, because a private second registration of a
// body another file owns is the drift that parameterisation exists to prevent.
//
// ---------------------------------------------------------------------------
// FAITHFULNESS NOTES A "TIDY" PORT WOULD DESTROY
// ---------------------------------------------------------------------------
//  * `?StartTimer@` finds an existing entry with the same id, rewrites its two
//    `Time` fields, AND THEN FALLS THROUGH AND APPENDS A SECOND ENTRY ANYWAY.
//    There is no `return` and no `else` at 0x1801137fd. Retail accumulates
//    duplicate timer ids and `?StopTimer@` removes only the first match.
//    WAVE7_LANE_B.md §6c wrote this down for whoever landed the body; this is
//    that landing, and the behaviour is reproduced, not repaired.
//  * The 4-arg `?AddController@` DEREFERENCES A NULL when the type id is not in
//    `Object::Factory`'s tree (0x1801133f4 sets RAX to the zero register and
//    0x1801133f7 immediately reads [RAX+0x10]), and again when the constructed
//    object is not a `UI::DialogController` (the `CMOVNZ` at 0x18011341e leaves
//    RDI at zero and 0x180113422 writes [RDI+0x10]). Both are reproduced.
//    CLAUDE.md: retail's unguarded derefs are load-bearing; a guard does not
//    restore the behaviour, it skips it.
//  * The 6-arg `?AddController@` calls `Array::SetSize` on `RBX+0xa0` and then
//    ORs `[RBX+0x34]` with RBX possibly null, for the same reason.
//  * The 6-arg's element-name test compares LENGTHS FIRST and only then calls
//    `StrCompare`, and its "lengths equal" arm re-tests the pointer and the
//    length a second time. Transcribed as written.
//  * `?StartTimer@`'s appended element leaves bytes +0x14..0x17 of the 0x18-byte
//    record UNWRITTEN — retail builds the fill record on the stack and never
//    touches that word. The differential oracle EXCLUDES those four bytes per
//    element; comparing them would fail 100 % of calls and read as a
//    catastrophic port defect
//    (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

// *** INCLUDE ORDER IS LOAD-BEARING. ***
// `runtime/types/object/External.h` opens `namespace RtObject {` and then
// `#include "TypeInfo.h"` INSIDE it, so whichever translation unit pulls
// External.h first nests every TypeInfo declaration into `RtObject::RtObject`
// for the whole TU — which is why dialog.cpp records that TypeInfo.h "IS NOT
// INCLUDABLE HERE". Including it FIRST fixes that for this TU: `#pragma once`
// then makes External.h's nested include a no-op. Do not move this line.
#include "../../types/object/TypeInfo.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_diff.h"
#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"
#include "../../types/ui/Dialog.h"
#include "../../types/ui/DialogInstance.h"

// ---- ResolveExport / ResolveAddr / ResolveRuntimeRaw -----------------------
// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: resolve.h drags in <windows.h>, whose <wingdi.h>
// declares a function called `Rectangle` and whose <windef.h> #defines
// `near`/`far` away. These declarations must stay byte-identical to
// resolve.h's; the linker checks the two against each other.
void* ResolveExport(const char* mangled_name);
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// The RAW door — the shipped DLL and nothing else. Needed by the displacement
// oracle, which must read the SHIPPED instruction stream and not our own
// prologue (reference_resolveaddr_consults_port_registry_first.md).
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// Wave 10 lane DLG2 (runtime/src/ui/dialog_ctor_dtor.cpp). See ProbeGate below.
extern "C" void RtUiDialogCtorDtorProbeNow();

using RtUI::Dialog;
using RtUI::DialogController;
using RtUI::DialogInstance;
using RtUI::DialogTimer;

namespace {

// ============================================================================
// Offsets this file needs that live in ANOTHER lane's class.
//
// runtime/types/ui/Element.h owns these and asserts every one of them; they are
// re-spelled as named constants rather than included because Element.h pulls
// the UI::String value type and this file has no other use for it. Each carries
// the instruction that pins it, which is the convention
// runtime/types/ui/UserInterface.h states for foreign offsets.
// ============================================================================
namespace EL {
enum : size_t {
    kName          = 0x10,   // ?SetName@   0x1801154f0            (Element.h)
    kFlags         = 0x34,   // ?GetElementFlags@ 0x1800354d0      (Element.h)
    kDialogParent  = 0x90,   // ctor 0x180114965: root->[0x90] = this
    kParent        = 0x98,   // ?SetParent@ 0x1801154d0            (Element.h)
    kChildrenData  = 0xa0,   // ?Child@      0x180035540           (Element.h)
    kChildrenSize  = 0xa8,   // ?ChildCount@ 0x180035530           (Element.h)
};
}  // namespace EL

// Element::_flags bit the 6-arg AddController propagates up the parent chain.
// `OR dword ptr [RBX+0x34],0x8000000` at 0x180113620. Element.h calls bit 27
// kElementFlagNeedsSizeCalc.
constexpr uint32_t kElementNeedsSizeCalc = 0x8000000u;

// ---- The allocator boundary ------------------------------------------------
// BY ADDRESS, NOT BY NAME, and deliberately: see the banner. Identical to
// runtime/src/collection/array.cpp's pair, including the retry-a-miss contract
// (at cutover this address is supplied by the allocator's own arming path, and
// a caller that ran before that point would otherwise freeze a null into the
// static for the life of the process).
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn(bytes, align);
}

// ---- Probe scheduling ------------------------------------------------------
//
// *** THE DIFFERENTIAL MUST NOT RUN FROM THE REGISTER FUNCTION. *** That runs
// inside `RuntimePortInit()`, immediately after `ResolveInit()`'s LoadLibrary —
// at which point the DLL's `MemoryAllocator` has been CONSTRUCTED but not
// STARTED (`Engine::Startup` does that later). `?StartTimer@` allocates, so a
// probe there is exactly the "works on my boot order" hazard
// runtime/src/collection/array_internal.h describes and self-schedules around.
//
// Same fix, same reasoning: arm at registration, and let the FIRST call into
// any of the four ports run it. That moment is provably safe because the caller
// is itself in the middle of building a dialog.
//
// The SHAPE pins are a different matter — they read bytes and allocate nothing,
// so they stay at registration where they are cheapest and earliest.
bool g_probePending = false;
void RunStartTimerDifferential();
inline void ProbeGate() {
    if (!g_probePending) return;
    g_probePending = false;      // clear FIRST — the probe calls back in here
    RunStartTimerDifferential();
    // Wave 10 lane DLG2 piggybacks here, and the coupling is deliberate rather
    // than tidy. `??0Dialog@UI@@`/`??1Dialog@UI@@` are reached ONLY from
    // `RtUiDialogInstance_AddController6` below, and only on the `if (desc)`
    // branch — which a 20-second autostart boot does not take, so DLG2's own
    // gate never fired and its differential silently never ran. THIS gate does
    // fire, and it fires late enough that the DLL's allocator is up (which is
    // the whole reason DLG2's probe is armed rather than run at registration).
    // It is a no-op when lane DLG2 is switched off or has already probed.
    RtUiDialogCtorDtorProbeNow();
}

// ---- Byte cursor -----------------------------------------------------------
inline uint8_t* B(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }
inline const uint8_t* B(const void* p, size_t off) {
    return static_cast<const uint8_t*>(p) + off;
}
template <class T> inline T& F(void* p, size_t off) {
    return *reinterpret_cast<T*>(B(p, off));
}
template <class T> inline const T& F(const void* p, size_t off) {
    return *reinterpret_cast<const T*>(B(p, off));
}

// ============================================================================
// Callees, all through the SEAM (ResolveAddr / ResolveExport), never the raw
// door — so that when another lane ports one of these, our body follows it,
// which is what retail's own direct near-CALL into the same DLL amounted to.
// ============================================================================

// Collection::Array<DialogTimer(0x18)>::SetSize(int newSize, const T* fill)
// 0x180076250. Registered as the SAME BODY as 0x18005fb00 (kSpec_Node18).
void ArraySetSize18(void* arrayHdr, int32_t newSize, const void* fill) {
    using Fn = void (*)(void*, int32_t, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180076250));
    fn(arrayHdr, newSize, fill);
}

// Collection::Array<T*>::SetSize(int newSize, const T* const* fill)
// 0x180076370. Registered as the SAME BODY as 0x180076900 (kSpec_Fill8Z).
void ArraySetSize8_76370(void* arrayHdr, int32_t newSize, const void* fill) {
    using Fn = void (*)(void*, int32_t, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180076370));
    fn(arrayHdr, newSize, fill);
}

// The SAME shape at 0x180076900 — already registered by array.cpp under
// kSpec_Fill8Z. The 6-arg AddController calls THIS address, not 0x180076370.
void ArraySetSize8_76900(void* arrayHdr, int32_t newSize, const void* fill) {
    using Fn = void (*)(void*, int32_t, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180076900));
    fn(arrayHdr, newSize, fill);
}

// ??0Internal@Object@@QEAA@XZ   0x1800bfd10   (an EXPORT — checked)
void* InternalCtor(void* self) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("??0Internal@Object@@QEAA@XZ"));
    return fn(self);
}

// ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z  0x1800031f0
int32_t GetSpriteIndexFromName(const void* sheet, const void* nameString) {
    using Fn = int32_t (*)(const void*, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport(
        "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"));
    return fn(sheet, nameString);
}

// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z  0x1800c0390  (PORTED, Wave 3 lane A —
// which is exactly why this goes through the seam and not the raw address.)
bool IsDerivedFrom(const void* obj, uint32_t typeId) {
    using Fn = bool (*)(const void*, uint32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    return fn(obj, typeId);
}

// ?StrCompare@String@System@@SAHPEBD0@Z  0x1800c98a0
int32_t StrCompare(const char* a, const char* b) {
    using Fn = int32_t (*)(const char*, const char*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("?StrCompare@String@System@@SAHPEBD0@Z"));
    return fn(a, b);
}

// ?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z
// 0x180115650. HIDDEN RETURN SLOT in RDX, returns that same pointer in RAX
// (reference_hidden_retslot_wild_write.md — an fn_* typedef that omits the slot
// is a wild write that still returns the right answer).
void** ElementFindElement(void* self, void** out, const void* nameString) {
    using Fn = void** (*)(void*, void**, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport(
        "?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@"
        "AEBVString@System@@@Z"));
    return fn(self, out, nameString);
}

// ??0Dialog@UI@@QEAA@XZ  0x180112db0 / ??1Dialog@UI@@UEAA@XZ  0x180112e40.
// The 6-arg AddController builds a default UI::Dialog on its stack with both
// bodies INLINED; the inlined sequences were diffed against these two exports
// instruction for instruction and are the same code (lane doc §3). Calling the
// exports IS retail's body minus the inlining — array.cpp's MemAlloc argument.
void* DialogCtor(void* self) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("??0Dialog@UI@@QEAA@XZ"));
    return fn(self);
}
void DialogDtor(void* self) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("??1Dialog@UI@@UEAA@XZ"));
    fn(self);
}

// The two `?_typeInfo@…@0VBaseTypeInfo@Object@@A` statics the bodies read a
// type id out of. Retail loads the FIRST DWORD of the static
// (`MOV EDX,dword ptr [0x1801f9e48]`), which BaseTypeInfo.h calls `typeId`.
//
// *** AEAA/0V… MEANS PRIVATE, NOT UNEXPORTED. *** This DLL exports its private
// members and both of these ARE in the export table
// (reference_aeaa_is_private_not_unexported.md), so they are resolved BY NAME
// and there is no address door here to justify.
//
// WAVE 10C lane TIDENT -- the ATTENDANCE line, and why a promotion needs it.
//
// `ResolveExport` consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so once a
// `?_typeInfo@` row is promoted this helper reads OUR minted node instead of
// the DLL's. The value it takes out is the typeId, and our node's typeId is the
// same CRC the DLL's holds -- so a promotion is, correctly, invisible in
// behaviour. That is exactly what makes an A/B on it UNINFORMATIVE: a run in
// which nothing changes cannot tell a safe promotion from one that was never
// exercised (this file's own "WHY NOT ALL 104" rule, in reg_mint_promoted.cpp).
//
// This line resolves that, the way lane ELEM's `[ElemTid]` line did: it prints
// WHICH NODE was read, not just what came out. Across an A/B the pointer must
// move out of the DLL's data into our image while the id stays byte-identical
// -- and if the line never prints at all, the row has no live consumer on that
// route and must not be promoted.
uint32_t TypeIdOfStatic(const char* mangled, uint32_t* cache) {
    if (*cache) return *cache;
    if (const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveExport(mangled))) {
        *cache = *p;
        // The RAW pointer, deliberately unclassified. A "(ours)/(theirs)"
        // verdict here would need an image-range test this TU does not have,
        // and inventing one is how a lane ships a label that means nothing.
        // The A/B does the classifying, exactly as lane ELEM's [ElemTid] line
        // did: our image and the DLL sit in visibly different ranges.
        printf("[TidStatic] %s  node=%p id=0x%08x\n",
               mangled, (const void*)p, *cache);
    }
    return *cache;
}
uint32_t g_tidDialogController = 0;   // ?_typeInfo@DialogController@UI@@  0x1801f9e48
uint32_t g_tidGroupElement     = 0;   // ?_typeInfo@GroupElement@UI@@      0x1801fabd8

inline uint32_t TypeIdDialogController() {
    return TypeIdOfStatic("?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A",
                          &g_tidDialogController);
}
inline uint32_t TypeIdGroupElement() {
    return TypeIdOfStatic("?_typeInfo@GroupElement@UI@@0VBaseTypeInfo@Object@@A",
                          &g_tidGroupElement);
}

// ?_singleton@Factory@Object@@0PEAV12@EA  — an exported DATA symbol (the cell,
// not the Factory). PLAN.md §2.3 calls 0x1801fc6e8 a global that "has no
// export"; both halves of that are wrong, and type_system.cpp already says so.
RtObject::Factory* FactorySingleton() {
    static RtObject::Factory** cell = nullptr;
    if (!cell) cell = reinterpret_cast<RtObject::Factory**>(
        ResolveExport("?_singleton@Factory@Object@@0PEAV12@EA"));
    return cell ? *cell : nullptr;
}

// ============================================================================
// ?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z   0x1801137c0
//
//     void DialogInstance::StartTimer(int id, System::Time t)
//
// `System::Time` is ONE int64 tick count and is passed BY VALUE in R8 — proved
// by the body itself, which stores R8 into the record's +0x00 and reloads the
// SAME home slot for +0x08 (0x1801137eb / 0x1801137f3). Both fields get the
// same value; DialogInstance.h names them `_remaining` and `_period` and calls
// the second one "the reload", which is what an equal seed means.
//
// The loop cursor is a BYTE offset stepped by 0x18 (`ADD RAX,0x18`), and the id
// it compares lives at record+0x10 — the two facts DialogInstance.h pinned
// `DialogTimer` from, and the two the displacement oracle re-pins out of the
// shipped bytes below.
//
// THE FALL-THROUGH IS THE SHIPPED BEHAVIOUR. After rewriting a matching entry
// the body does NOT return; it walks to the end of the loop and appends another
// record with the same id (WAVE7_LANE_B.md §6c). Ported as written.
// ============================================================================
void RtUiDialogInstance_StartTimer(void* self, int32_t id, int64_t time) {
    ProbeGate();
    DialogInstance* const d = static_cast<DialogInstance*>(self);

    // 0x1801137d2: `CMP dword ptr [RCX+0x70],R10D` / `JLE` — a SIGNED test on
    // _timers._size, so a negative size skips the loop rather than wrapping.
    for (int32_t i = 0; i < d->_timers._size; ++i) {
        uint8_t* const rec = reinterpret_cast<uint8_t*>(d->_timers._data) +
                             static_cast<size_t>(i) * sizeof(DialogTimer);
        // The data pointer is RE-READ from the object on every iteration
        // (0x1801137e0 and 0x1801137ef both `MOV …,[R11+0x68]`). Kept.
        if (F<int32_t>(rec, offsetof(DialogTimer, _id)) == id) {
            F<int64_t>(rec, offsetof(DialogTimer, _remaining)) = time;
            uint8_t* const rec2 = reinterpret_cast<uint8_t*>(d->_timers._data) +
                                  static_cast<size_t>(i) * sizeof(DialogTimer);
            F<int64_t>(rec2, offsetof(DialogTimer, _period)) = time;
            // NO `break`, NO `return` — see the header block.
        }
    }

    // The fill record is built on the stack with THREE stores; +0x14..0x17 is
    // never written and the 0x18-byte byte-copy in the grow helper carries
    // whatever the stack held. `tmp` is deliberately left uninitialised there
    // for the same reason.
    DialogTimer tmp;
    tmp._remaining = time;
    tmp._period    = time;
    tmp._id        = id;
    ArraySetSize18(&d->_timers, d->_timers._size + 1, &tmp);
}

// ============================================================================
// ?AddController@DialogInstance@UI@@QEAAX I PEAVBase@Object@@
//     V?$ExternalPtr@VDialogControllerConfig@UI@@@4@
//     V?$InternalPtr@VElement@UI@@@4@ @Z                       0x1801133b0
//
//     void DialogInstance::AddController(unsigned typeId, Object::Base* dataSource,
//                                        ExternalPtr<DialogControllerConfig> config,
//                                        InternalPtr<Element> container)
//
// ARGUMENT ABI, read off the homing stores rather than from the mangled name:
//   RCX  this
//   EDX  typeId
//   R8   Object::Base* dataSource          -> controller+0x28
//   R9   the ExternalPtr's 8 bytes BY VALUE -> controller+0x10
//   [rsp+0x28]  a POINTER to the InternalPtr — 0x180113416 loads the argument
//   and 0x18011341b DEREFERENCES it before storing. engine_funcs.h's
//   `fn_DialogInstance_AddControllerTab_t` already spells it that way and its
//   exe call sites pass an address; the two agree.
//
// THE TWO UNGUARDED NULL DEREFERENCES ARE RETAIL'S. See the banner.
// ============================================================================
void RtUiDialogInstance_AddControllerTab(void* self, uint32_t typeId, void* dataSource,
                                         void* config, void** containerPtr) {
    // ---- Object::Factory's AVL find, transcribed from 0x1801133c1 ----------
    // Descend LEFT on `id < key` (`CMP EDX,R9D / JC`), stop on equal
    // (`CMP R9D,EDX / JNC`), otherwise RIGHT. Identical to the walk
    // runtime/src/object/type_system.cpp already ports as `FindNode`; spelled
    // out here because retail inlined it and the port reproduces the inlining.
    RtObject::Factory* const factory = FactorySingleton();
    RtObject::TypeNode* node = factory ? factory->root : nullptr;
    RtObject::TypeNode* found = nullptr;
    while (node) {
        const uint32_t key = node->key;
        if (typeId < key)      node = node->left;
        else if (key >= typeId) { found = node; break; }
        else                    node = node->right;
    }

    // 0x1801133f7 `MOV RCX,[RAX+0x10]` with RAX possibly zero. TypeNode+0x10 is
    // `value.info`, the BaseTypeInfo*. FAITHFUL: no guard (CLAUDE.md).
    RtObject::BaseTypeInfo* const info =
        *reinterpret_cast<RtObject::BaseTypeInfo* const*>(B(found, 0x10));

    // 0x1801133fb `CALL qword ptr [RCX+0x20]` — the call goes through
    // info+0x20 (`factoryFn`), and RCX is left holding `info`. The callee is
    // `?CreateNewObject@…@SAPEAVBase@Object@@XZ`, a STATIC that ignores it; the
    // argument is passed anyway so the register state matches retail exactly.
    void* const created =
        reinterpret_cast<void* (*)(void*)>(info->factoryFn)(info);

    // 0x1801133fe: the type id comes from the FIRST DWORD of
    // ?_typeInfo@DialogController@UI@@, not from a literal.
    const bool isController = IsDerivedFrom(created, TypeIdDialogController());

    // 0x18011341e `CMOVNZ RDI,RBX` with RDI still zero — so a NON-DialogController
    // leaves the target null and the four stores below write through null.
    DialogController* const c =
        static_cast<DialogController*>(isController ? created : nullptr);

    // Retail loads the container BEFORE the CMOV (0x180113416/0x18011341b), so
    // the deref of `containerPtr` happens on both arms. Kept in that order.
    void* const container = *containerPtr;

    c->_config     = config;       // +0x10  0x180113422
    c->_dataSource = dataSource;   // +0x28  0x180113426
    c->_dialog     = self;         // +0x18  0x18011342a
    c->_container  = container;    // +0x20  0x18011342e

    // 0x18011343d `CALL qword ptr [RAX+0x20]` where RAX = *(void**)c — a REAL
    // vtable dispatch this time (slot 4), with `this` and nothing else.
    void* const* const vt = *reinterpret_cast<void* const* const*>(c);
    reinterpret_cast<void (*)(void*)>(vt[4])(c);

    // 0x180113440: append to _controllers through the stride-8 grow helper.
    // The fill is the ADDRESS OF A STACK SLOT holding the pointer, not the
    // pointer — `LEA R8,[RSP+0x40]` where [RSP+0x40] was stored at 0x180113438.
    DialogInstance* const d = static_cast<DialogInstance*>(self);
    void* slot = c;
    ArraySetSize8_76370(&d->_controllers, d->_controllers._size + 1, &slot);
}

// ============================================================================
// ?AddController@DialogInstance@UI@@QEAAX I PEAVBase@Object@@
//     V?$ExternalPtr@VDialogControllerConfig@UI@@@4@
//     V?$ExternalPtr@VElementDescription@UI@@@4@
//     AEBVString@System@@ @Z                                   0x180113470
//
//     void DialogInstance::AddController(unsigned typeId, Object::Base* dataSource,
//                                        ExternalPtr<DialogControllerConfig> config,
//                                        ExternalPtr<ElementDescription> desc,
//                                        const System::String& name)
//
// Three phases:
//   1. RESOLVE the insertion element by name — the dialog root itself if its
//      name matches, else the first hit of `Element::FindElement` over the
//      root's direct children, else null; then require it to be a
//      `UI::GroupElement` (`?_typeInfo@GroupElement@UI@@`, 0x1801fabd8).
//   2. CREATE the element: default-construct a temporary `UI::Dialog`, call the
//      description's vtable slot 8 with (out, &tmpDialog), parent the result
//      into the resolved element, append it to that element's `_children`, and
//      OR kElementNeedsSizeCalc up the whole parent chain. Then destroy tmp.
//   3. FORWARD to the 4-arg overload with the created element, if id != 0.
//
// THE TEMPORARY IS A `UI::Dialog`, NOT AN `ElementDescription`. The inlined
// ctor stamps `??_7Dialog@UI@@6B@` (0x1801562a0) as its third vtable — the
// ElementDescription ctor stamps 0x1801561c0. Getting that wrong would have put
// the wrong object on the description's slot-8 call. See lane doc §3.
//
// WHEN `desc` IS NULL the whole of phases 1 and 2 is skipped (0x1801134aa) and
// the body goes straight to phase 3 with a null element.
// ============================================================================
void RtUiDialogInstance_AddController6(void* self, uint32_t typeId, void* dataSource,
                                       void* config, void* desc, const void* nameString) {
    void* createdElement = nullptr;   // RDI

    if (desc) {
        DialogInstance* const d = static_cast<DialogInstance*>(self);
        void* insertion = d->_root;   // RDI starts at this->_root (0x1801134b4)

        // ---- phase 1a: does the ROOT's own name match? ----------------------
        // `name` is `const String&`, so `nameString` IS the String object and
        // its single member is the refcounted `char*`. Length lives at ptr-8.
        const char* const want = *reinterpret_cast<const char* const*>(nameString);
        bool useRoot = false;
        if (want && *reinterpret_cast<const int32_t*>(want - 8) != 0) {
            // 0x1801134cb `MOV RCX,[RDI+0x10]` — UNGUARDED on `insertion`.
            // `insertion` is `this->_root`, which the ctor already dereferences
            // unguarded at +0x90, so retail's assumption holds by construction.
            // No guard added: it would not restore the behaviour, it would skip
            // it (CLAUDE.md).
            const char* const have =
                *reinterpret_cast<const char* const*>(B(insertion, EL::kName));
            const int32_t haveLen =
                have ? *reinterpret_cast<const int32_t*>(have - 8) : 0;
            if (haveLen == *reinterpret_cast<const int32_t*>(want - 8)) {
                // Retail re-tests `have` and its length here (0x1801134e2 and
                // 0x1801134e7) before calling StrCompare. Both re-tests jump to
                // the SAME "use it" label, so an empty-but-equal-length name
                // matches without a compare. Transcribed as written.
                if (!have) useRoot = true;
                else if (*reinterpret_cast<const int32_t*>(have - 8) == 0) useRoot = true;
                else if (StrCompare(have, want) == 0) useRoot = true;
            }
        }

        // ---- phase 1b: otherwise scan the root's DIRECT children ------------
        if (!useRoot) {
            // 0x1801134f5 `MOV R14D,[RDI+0xa8]` — unguarded, same argument.
            const int32_t n =
                *reinterpret_cast<const int32_t*>(B(insertion, EL::kChildrenSize));
            void* hit = nullptr;
            if (n > 0) {
                void** kids = *reinterpret_cast<void***>(B(insertion, EL::kChildrenData));
                for (int32_t i = 0; i < n; ++i) {
                    void* const child = kids[i];
                    if (child) {
                        void* out = nullptr;
                        ElementFindElement(child, &out, nameString);
                        if (out) { hit = out; break; }
                    }
                }
            }
            // 0x180113538: a miss zeroes RDI through a 32-bit `MOV EDI,ESI`,
            // which zero-extends and clears the whole register. Ghidra prints
            // that as a plain assignment; the disassembly is what says it kills
            // the upper half (reference_ghidra_c_output_vs_disassembly.md).
            insertion = hit;
        }

        // ---- the GroupElement gate -----------------------------------------
        // `CMOVNZ RBX,RDI` with RBX pre-loaded from the zero register: a
        // non-GroupElement (or a miss) leaves the insertion point NULL and the
        // stores below run through null, exactly as retail does.
        void* const target =
            IsDerivedFrom(insertion, TypeIdGroupElement()) ? insertion : nullptr;

        // ---- phase 2: build the element -------------------------------------
        // A default-constructed UI::Dialog on OUR stack, byte-for-byte what
        // retail inlines. `alignas(16)` because the inlined ctor zeroes +0x20
        // with a MOVDQA, i.e. retail's temporary is 16-byte aligned.
        alignas(16) uint8_t tmpDialog[0x90];
        DialogCtor(tmpDialog);

        void* newEl = nullptr;
        {
            // 0x1801135db: RCX is the ExternalPtr's TARGET, RDX the hidden
            // return slot, R8 the Dialog*. Slot +0x40 == index 8.
            void* const* const vt = *reinterpret_cast<void* const* const*>(desc);
            void** const ret = reinterpret_cast<void** (*)(void*, void**, void*)>(vt[8])(
                desc, &newEl, tmpDialog);
            newEl = *ret;   // 0x1801135ef `MOV RDI,qword ptr [RAX]`
        }
        createdElement = newEl;

        if (newEl) F<void*>(newEl, EL::kParent) = target;   // 0x1801135fc

        // 0x180113603: unguarded on `target`. The fill is the ADDRESS of the
        // slot holding the pointer (0x1801135f2 stored it there first).
        ArraySetSize8_76900(B(target, EL::kChildrenData),
                            *reinterpret_cast<int32_t*>(B(target, EL::kChildrenSize)) + 1,
                            &newEl);

        // 0x180113620: a DO-WHILE — the flag is set on `target` itself before
        // the null test, so a null target faults here rather than skipping.
        void* walk = target;
        do {
            F<uint32_t>(walk, EL::kFlags) |= kElementNeedsSizeCalc;
            walk = F<void*>(walk, EL::kParent);
        } while (walk);

        DialogDtor(tmpDialog);
    }

    // ---- phase 3 ------------------------------------------------------------
    // 0x1801136e5: `TEST R12D,R12D` — id 0 means "element only, no controller".
    // The InternalPtr is handed on BY ADDRESS, which is what the 4-arg overload
    // dereferences.
    if (typeId != 0) {
        void* slot = createdElement;
        using Fn = void (*)(void*, uint32_t, void*, void*, void**);
        static Fn fn = nullptr;
        if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1801133b0));
        fn(self, typeId, dataSource, config, &slot);
    }
}

// ============================================================================
// ??0DialogInstance@UI@@QEAA@ PEAVUserInterface@1@ PEBVDialog@1@
//     V?$InternalPtr@VDialogInstance@UI@@@Object@@ PEAVBase@5@ PEAX H @Z
//                                                              0x180114870
//
//     DialogInstance::DialogInstance(UserInterface* ui, const Dialog* dialog,
//                                    InternalPtr<DialogInstance> parent,
//                                    Object::Base* dataSource,
//                                    void* userData, int userFlags)
//
// `parent` is an InternalPtr passed BY HIDDEN REFERENCE (R9 is dereferenced at
// 0x1801148c9), which is how engine_funcs.h's `fn_DialogInstance_Ctor_t` already
// spells it and how all sixteen exe call sites pass it.
//
// Returns `this` in RAX (0x180114a37). MSVC always does for a ctor; the port
// must produce it because the exe's call sites are typed to receive it.
//
// Every field offset below is asserted in runtime/types/ui/DialogInstance.h and
// the four holes the ctor never writes (+0x4c, +0x64, +0x99..0x9f, +0xb4) are
// named there. This body writes exactly the fields it is shown writing; it does
// not zero the holes.
// ============================================================================
void* RtUiDialogInstance_Ctor(void* self, void* ui, const void* dialog,
                              void** parentPtr, void* dataSource,
                              void* userData, int32_t userFlags) {
    ProbeGate();
    DialogInstance* const d = static_cast<DialogInstance*>(self);
    const Dialog* const dlg = static_cast<const Dialog*>(dialog);

    InternalCtor(self);                                       // 0x1801148a2

    // 0x1801148a8: the DLL's OWN exported vtable, never a minted one. This is
    // the `??_7Geometry@Graphics@@6B@` lever from Wave 6 — resolving retail's
    // table keeps a half-ported class from putting two vtables in play.
    static void* vtbl = nullptr;
    if (!vtbl) vtbl = ResolveExport("??_7DialogInstance@UI@@6B@");
    F<void*>(d, 0x00) = vtbl;

    d->_ui         = ui;                          // +0x10
    d->_dialog     = const_cast<void*>(dialog);   // +0x18
    d->_flags      = dlg->_defaultFlags;          // +0x20  from _dialog->+0x40
    d->_flags2     = 0;                           // +0x24
    d->_parent     = nullptr;                     // +0x28  written 0 then...
    d->_parent     = *parentPtr;                  // +0x28  ...the real value
    d->_dataSource = dataSource;                  // +0x30
    d->_children._head  = nullptr;                // +0x38
    d->_children._tail  = nullptr;                // +0x40
    d->_children._count = 0;                      // +0x48   (+0x4c left alone)

    // 0x1801148e7: `_dialog->_rootDescription`'s vtable slot 8, with the hidden
    // return slot pointed AT THE OBJECT'S OWN `_root` FIELD and the Dialog as
    // the second argument — the same slot the 6-arg AddController calls.
    {
        void* const rootDesc = dlg->_rootDescription;
        void* const* const vt = *reinterpret_cast<void* const* const*>(rootDesc);
        reinterpret_cast<void** (*)(void*, void**, const void*)>(vt[8])(
            rootDesc, &d->_root, dialog);
    }

    d->_spriteSheet = dlg->_spriteSheet;           // +0x58
    // 0x180114900: -1 when the sheet is null, otherwise the sheet's lookup of
    // `&_dialog->_spriteName` — the String OBJECT's address, not the char*.
    d->_spriteIndex = d->_spriteSheet
        ? GetSpriteIndexFromName(d->_spriteSheet, &dlg->_spriteName)
        : -1;                                      // +0x60   (+0x64 left alone)

    d->_timers._data      = nullptr;               // +0x68
    d->_timers._size      = 0;                     // +0x70
    d->_timers._capacity  = 0;                     // +0x74
    d->_controllers._data     = nullptr;           // +0x78
    d->_controllers._size     = 0;                 // +0x80
    d->_controllers._capacity = 0;                 // +0x84
    d->_upperLeftX = 0;                            // +0x88
    d->_upperLeftY = 0;                            // +0x8c
    d->_timerHandle = -1;                          // +0x90  a QWORD -1
    d->_hovering    = 0;                           // +0x98  a single BYTE
    d->_hoverTime   = 0;                           // +0xa0   (+0x99.. left alone)
    d->_userData    = userData;                    // +0xa8
    d->_userFlags   = userFlags;                   // +0xb0   (+0xb4 left alone)

    // 0x180114965: the freshly created root Element points back at us.
    // UNGUARDED in retail — if slot 8 returned null this faults. Faithful.
    F<void*>(d->_root, EL::kDialogParent) = self;

    // ---- link into the parent's child list -------------------------------
    // A PREPEND: head at +0x38, tail at +0x40, node = {element, prev, next}
    // with prev at +0x08 and next at +0x10. DialogInstance.h records the
    // tie-break (`?GetFirstDialog@UserInterface@UI@@`, 0x180031150, reads the
    // FIRST element from the list's +0x00) and warns that reading the ctor
    // alone gets this backwards.
    if (DialogInstance* const parent = static_cast<DialogInstance*>(d->_parent)) {
        // 0x180114999: `MOV EDX,0x18 / LEA R8D,[RDX-0x10]` -> Allocate(0x18, 8),
        // wrapped in Enter/Leave — i.e. exactly ?MemAlloc@…(0x18, 8).
        uint8_t* node = static_cast<uint8_t*>(MemAlloc(0x18, 8));
        if (node) {
            F<void*>(node, 0x00) = self;      // element
            F<void*>(node, 0x08) = nullptr;   // prev
            F<void*>(node, 0x10) = nullptr;   // next
        }
        // 0x1801149cf: on an OOM `node` is null and retail links the null in
        // anyway. Reproduced, guard-free.
        if (void* head = parent->_children._head) {
            F<void*>(head, 0x08) = node;                      // old head->prev
            F<void*>(node, 0x10) = parent->_children._head;   // node->next
        } else {
            parent->_children._tail = reinterpret_cast<decltype(parent->_children._tail)>(node);
        }
        parent->_children._head = reinterpret_cast<decltype(parent->_children._head)>(node);
        parent->_children._count++;
    }

    // ---- run the dialog's controller table -------------------------------
    // Stride 0x20 (`ADD RDI,0x20`).
    //
    // *** THE ARGUMENT MAPPING IS THE OPPOSITE OF THE FIELD NAMES, AND THIS
    //     PORT HAD IT BACKWARDS UNTIL THE DISASSEMBLY WAS RE-READ. ***
    //
    //     0x180114a13  MOV RAX,[R9+0x08]  ->  0x180114a17  [RSP+0x20]  = ARG 5
    //     0x180114a1c  MOV R9,[R9+0x10]                                = ARG 4
    //
    // Argument 4 is the `ExternalPtr<DialogControllerConfig>` and argument 5 is
    // the `ExternalPtr<ElementDescription>`, so entry+0x10 is the CONFIG and
    // entry+0x08 is the DESCRIPTION.
    //
    // *** 2026-09-08, lane DLG2: THE HEADER IS NOW CORRECT AND THIS CALL SITE
    //     NO LONGER COMPENSATES. *** `runtime/types/ui/Dialog.h` used to name
    //     `_config` at +0x08 and `_elementDesc` at +0x10, so this loop passed
    //     them crossed over to get the right bytes into the right registers.
    //     The header now names them the way the two `MOV`s above prove, and the
    //     arguments below read straight. THE BYTES DID NOT CHANGE; only the
    //     spelling did. The three PIN lines at the bottom of this file check
    //     exactly this mapping against the shipped stream, so a re-swap in
    //     either place fails loudly rather than silently.
    for (int32_t i = 0; i < dlg->_controllerCount; ++i) {
        const RtUI::DialogControllerConfigEntry& e = dlg->_controllerConfigs[i];
        using Fn = void (*)(void*, uint32_t, void*, void* /*arg4: config*/,
                            void* /*arg5: description*/, const void*);
        static Fn fn = nullptr;
        if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180113470));
        fn(self, e._typeId, d->_dataSource,
           e._config,        // entry+0x10 -> R9  -> ExternalPtr<DialogControllerConfig>
           e._elementDesc,   // entry+0x08 -> a5  -> ExternalPtr<ElementDescription>
           &e._name);        // entry+0x18 -> a6  -> const String&
    }

    return self;   // 0x180114a37 `MOV RAX,RSI`
}

// ============================================================================
// ORACLE (b) — SHIPPED-BYTE DISPLACEMENT PINS
//
// Every structural constant these four bodies compile in is a literal in the
// shipped instruction stream. `PinDisp` finds the instruction BY PATTERN,
// requires the pattern to occur EXACTLY ONCE in the body (so it cannot silently
// decode the wrong four bytes — the failure that made uic_bytediff.py's first
// version print a confident wrong mismatch), and compares the value that
// follows against the constant this port uses.
//
// The authority is the DLL. This is not RT_DIFF_SHADOW.
// ============================================================================

struct PinCtx {
    const uint8_t* body;
    size_t         len;
    RtDiffSite*    site;
    int            checked;
    int            failed;
};

// Find `pat` in the body. Returns the single occurrence's offset, or -1 for
// "absent", or -2 for "ambiguous" — the two are reported differently on
// purpose, because "not unique" is a defect in the PATTERN and "absent" is a
// defect in the CLAIM.
long FindUnique(const PinCtx& c, const uint8_t* pat, size_t n) {
    long at = -1;
    for (size_t i = 0; i + n <= c.len; ++i) {
        if (std::memcmp(c.body + i, pat, n) == 0) {
            if (at >= 0) return -2;
            at = static_cast<long>(i);
        }
    }
    return at;
}

// PRESENT: the bytes `pat` occur exactly once. Used where a short "read the
// displacement that follows" pattern is genuinely ambiguous inside the body —
// then the value goes INSIDE the pattern and the claim becomes "the shipped
// body contains exactly this instruction, once". Weaker than PinDisp (it cannot
// print what the DLL says instead), and it says so in its own message rather
// than being dressed up as the stronger check.
void PinPresent(PinCtx& c, const uint8_t* pat, size_t n, const char* what) {
    ++c.checked;
    const long at = FindUnique(c, pat, n);
    char ours[192], theirs[192];
    std::snprintf(ours, sizeof(ours), "%s (port; PRESENCE-only pin)", what);
    if (at < 0) {
        ++c.failed;
        std::snprintf(theirs, sizeof(theirs), "%s in the shipped body",
                      at == -2 ? "NOT UNIQUE" : "NOT FOUND");
        RtDiffReport(c.site, false, ours, theirs);
        return;
    }
    std::snprintf(theirs, sizeof(theirs), "present exactly once at body+0x%lx", at);
    RtDiffReport(c.site, true, ours, theirs);
}

// PIN: the bytes `pat` occur exactly once, and the `width`-byte little-endian
// field immediately after them equals `expected`.
void PinDisp(PinCtx& c, const uint8_t* pat, size_t n, int width,
             int64_t expected, const char* what) {
    ++c.checked;
    const long at = FindUnique(c, pat, n);
    char ours[192], theirs[192];
    if (at < 0) {
        std::snprintf(ours, sizeof(ours), "%s = %lld (port)", what,
                      static_cast<long long>(expected));
        std::snprintf(theirs, sizeof(theirs), "pattern %s in the shipped body",
                      at == -2 ? "NOT UNIQUE" : "NOT FOUND");
        ++c.failed;
        RtDiffReport(c.site, false, ours, theirs);
        return;
    }
    int64_t got = 0;
    const uint8_t* p = c.body + at + n;
    for (int i = 0; i < width; ++i) got |= static_cast<int64_t>(p[i]) << (8 * i);
    // sign-extend, because every displacement in these bodies is signed
    if (width == 1 && (got & 0x80))        got |= ~static_cast<int64_t>(0xff);
    if (width == 4 && (got & 0x80000000LL)) got |= ~static_cast<int64_t>(0xffffffffLL);

    const bool eq = (got == expected);
    if (!eq) ++c.failed;
    std::snprintf(ours,   sizeof(ours),   "%s = 0x%llx (port)", what,
                  static_cast<unsigned long long>(expected));
    std::snprintf(theirs, sizeof(theirs), "%s = 0x%llx (shipped, at body+0x%lx)", what,
                  static_cast<unsigned long long>(got), at + n);
    RtDiffReport(c.site, eq, ours, theirs);
}

// PIN a rel32 CALL: the byte at `at` is 0xE8 and its target is `expected`.
void PinCall(PinCtx& c, const uint8_t* pat, size_t n, uintptr_t bodyVa,
             uintptr_t expected, const char* what) {
    ++c.checked;
    const long at = FindUnique(c, pat, n);
    char ours[192], theirs[192];
    std::snprintf(ours, sizeof(ours), "%s -> 0x%llx (port)", what,
                  static_cast<unsigned long long>(expected));
    if (at < 0) {
        ++c.failed;
        std::snprintf(theirs, sizeof(theirs), "pattern %s in the shipped body",
                      at == -2 ? "NOT UNIQUE" : "NOT FOUND");
        RtDiffReport(c.site, false, ours, theirs);
        return;
    }
    const size_t off = static_cast<size_t>(at) + n;      // points at the 0xE8
    int32_t rel = 0;
    std::memcpy(&rel, c.body + off + 1, 4);
    const uintptr_t target = bodyVa + off + 5 + static_cast<intptr_t>(rel);
    const bool eq = (c.body[off] == 0xE8) && (target == expected);
    if (!eq) ++c.failed;
    std::snprintf(theirs, sizeof(theirs), "%s -> 0x%llx (shipped)", what,
                  static_cast<unsigned long long>(target));
    RtDiffReport(c.site, eq, ours, theirs);
}

// The four sites. Each reports ONCE, at registration.
//
// EVERY PATTERN BELOW WAS HAND-ENCODED FROM THE LISTING AND THEN REQUIRED TO BE
// UNIQUE. Where a short pattern turned out to be ambiguous inside the body (the
// AVL `MOV RAX,[RAX+disp8]` pair is the same three bytes for LEFT and RIGHT),
// the pattern was LENGTHENED with the neighbouring instruction rather than
// shortened into a hardcoded byte offset — the latter is exactly what made
// uic_bytediff.py's first version decode four bytes one position off and print
// a confident wrong mismatch.
void VerifyStartTimerShape(RtDiffSite* site) {
    RtDiffRegister(site);
    const uint8_t* body = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x1801137c0));
    if (!body) return;
    PinCtx c = { body, 0x77, site, 0, 0 };

    // `CMP dword ptr [RCX+0x70],R10D`   -> _timers._size at +0x70
    { static const uint8_t p[] = { 0x44, 0x39, 0x51 }; PinDisp(c, p, 3, 1,
        offsetof(DialogInstance, _timers) + 8, "DialogInstance::_timers._size"); }
    // `MOV R9,qword ptr [R11+0x68]`     -> _timers._data at +0x68
    { static const uint8_t p[] = { 0x4d, 0x8b, 0x4b }; PinDisp(c, p, 3, 1,
        offsetof(DialogInstance, _timers), "DialogInstance::_timers._data"); }
    // `CMP dword ptr [R9+RAX*1+0x10],EBX` -> DialogTimer::_id at +0x10
    { static const uint8_t p[] = { 0x41, 0x39, 0x5c, 0x01 }; PinDisp(c, p, 4, 1,
        offsetof(DialogTimer, _id), "DialogTimer::_id"); }
    // `MOV qword ptr [RDX+RAX*1+0x8],RCX` -> DialogTimer::_period at +0x08
    { static const uint8_t p[] = { 0x48, 0x89, 0x4c, 0x02 }; PinDisp(c, p, 4, 1,
        offsetof(DialogTimer, _period), "DialogTimer::_period"); }
    // `ADD RAX,0x18`                      -> the element stride
    { static const uint8_t p[] = { 0x48, 0x83, 0xc0 }; PinDisp(c, p, 3, 1,
        static_cast<int64_t>(sizeof(DialogTimer)), "DialogTimer stride"); }
    // the grow helper
    { static const uint8_t p[] = { 0x89, 0x5c, 0x24, 0x30 }; PinCall(c, p, 4,
        0x1801137c0, 0x180076250, "Array<0x18>::SetSize"); }

    printf("[RtPort]   StartTimer[shape]: %d pins, %d failed\n", c.checked, c.failed);
}

void VerifyAddControllerTabShape(RtDiffSite* site) {
    RtDiffRegister(site);
    const uint8_t* body = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x1801133b0));
    if (!body) return;
    PinCtx c = { body, 0xb0, site, 0, 0 };

    // The AVL links. `MOV RAX,[RAX+0x40]` and `MOV RAX,[RAX+0x38]` are the same
    // three opening bytes, so each is anchored on the branch in front of it:
    //   0x1801133e3  JNC +0x12          ->  0x1801133e5  MOV RAX,[RAX+0x40]  RIGHT
    //   0x1801133e9  JMP +0x04          ->  0x1801133eb  MOV RAX,[RAX+0x38]  LEFT
    { static const uint8_t p[] = { 0x73, 0x12, 0x48, 0x8b, 0x40 }; PinDisp(c, p, 5, 1,
        offsetof(RtObject::TypeNode, right), "TypeNode::right"); }
    { static const uint8_t p[] = { 0xeb, 0x04, 0x48, 0x8b, 0x40 }; PinDisp(c, p, 5, 1,
        offsetof(RtObject::TypeNode, left), "TypeNode::left"); }
    // `MOV RCX,qword ptr [RAX+0x10]` -> TypeNode::value.info (the BaseTypeInfo*)
    { static const uint8_t p[] = { 0x48, 0x8b, 0x48 }; PinDisp(c, p, 3, 1,
        offsetof(RtObject::TypeNode, value) + offsetof(RtObject::TypeRecord, info),
        "TypeNode::value.info"); }
    // `CALL qword ptr [RCX+0x20]` -> BaseTypeInfo::factoryFn
    { static const uint8_t p[] = { 0xff, 0x51 }; PinDisp(c, p, 2, 1,
        offsetof(RtObject::BaseTypeInfo, factoryFn), "BaseTypeInfo::factoryFn"); }
    // `MOV qword ptr [RDI+0x10],RDX` -> DialogController::_config
    { static const uint8_t p[] = { 0x48, 0x89, 0x57 }; PinDisp(c, p, 3, 1,
        offsetof(DialogController, _config), "DialogController::_config"); }
    // `MOV qword ptr [RDI+0x28],RBP` -> _dataSource
    { static const uint8_t p[] = { 0x48, 0x89, 0x6f }; PinDisp(c, p, 3, 1,
        offsetof(DialogController, _dataSource), "DialogController::_dataSource"); }
    // `MOV qword ptr [RDI+0x18],RSI` -> _dialog
    { static const uint8_t p[] = { 0x48, 0x89, 0x77 }; PinDisp(c, p, 3, 1,
        offsetof(DialogController, _dialog), "DialogController::_dialog"); }
    // `MOV qword ptr [RDI+0x20],RAX` -> _container
    { static const uint8_t p[] = { 0x48, 0x89, 0x47 }; PinDisp(c, p, 3, 1,
        offsetof(DialogController, _container), "DialogController::_container"); }
    // `LEA RCX,[RSI+0x78]` -> DialogInstance::_controllers
    { static const uint8_t p[] = { 0x48, 0x8d, 0x4e }; PinDisp(c, p, 3, 1,
        offsetof(DialogInstance, _controllers), "DialogInstance::_controllers"); }
    // `LEA R8,[RSP+0x40]` `INC EDX` then the rel32 CALL.
    { static const uint8_t p[] = { 0x4c, 0x8d, 0x44, 0x24, 0x40, 0xff, 0xc2 };
      PinCall(c, p, 7, 0x1801133b0, 0x180076370, "Array<ptr>::SetSize"); }

    printf("[RtPort]   AddController(tab)[shape]: %d pins, %d failed\n",
                c.checked, c.failed);
}

void VerifyAddController6Shape(RtDiffSite* site) {
    RtDiffRegister(site);
    const uint8_t* body = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x180113470));
    if (!body) return;
    PinCtx c = { body, 0x2b2, site, 0, 0 };

    // `MOV RDI,qword ptr [RCX+0x50]` -> DialogInstance::_root
    { static const uint8_t p[] = { 0x48, 0x8b, 0x79 }; PinDisp(c, p, 3, 1,
        offsetof(DialogInstance, _root), "DialogInstance::_root"); }
    // `MOV RCX,qword ptr [RDI+0x10]` -> Element::_name
    { static const uint8_t p[] = { 0x48, 0x8b, 0x4f }; PinDisp(c, p, 3, 1,
        static_cast<int64_t>(EL::kName), "Element::_name"); }
    // `MOV R14D,dword ptr [RDI+0xa8]` -> Element::_children._size
    { static const uint8_t p[] = { 0x44, 0x8b, 0xb7 }; PinDisp(c, p, 3, 4,
        static_cast<int64_t>(EL::kChildrenSize), "Element::_children._size"); }
    // `MOV RBX,qword ptr [RDI+0xa0]` -> Element::_children._data
    { static const uint8_t p[] = { 0x48, 0x8b, 0x9f }; PinDisp(c, p, 3, 4,
        static_cast<int64_t>(EL::kChildrenData), "Element::_children._data"); }
    // `MOV qword ptr [RDI+0x98],RBX` -> Element::_parent
    { static const uint8_t p[] = { 0x48, 0x89, 0x9f }; PinDisp(c, p, 3, 4,
        static_cast<int64_t>(EL::kParent), "Element::_parent"); }
    // `OR dword ptr [RBX+0x34],0x8000000` -> Element::_flags and the bit
    { static const uint8_t p[] = { 0x81, 0x4b }; PinDisp(c, p, 2, 1,
        static_cast<int64_t>(EL::kFlags), "Element::_flags"); }
    { static const uint8_t p[] = { 0x81, 0x4b, 0x34 }; PinDisp(c, p, 3, 4,
        static_cast<int64_t>(kElementNeedsSizeCalc), "kElementNeedsSizeCalc"); }
    // `CALL qword ptr [RAX+0x40]` -> the description's vtable slot 8
    { static const uint8_t p[] = { 0xff, 0x50 }; PinDisp(c, p, 2, 1, 8 * 8,
        "ElementDescription vtable slot 8"); }
    // the two callees this body reaches by rel32 that we also call
    { static const uint8_t p[] = { 0x4d, 0x8b, 0xc7, 0x48, 0x8d, 0x54, 0x24, 0x30 };
      PinCall(c, p, 8, 0x180113470, 0x180115650, "Element::FindElement"); }
    { static const uint8_t p[] = { 0x4c, 0x8d, 0x44, 0x24, 0x30 }; PinCall(c, p, 5,
        0x180113470, 0x180076900, "Array<ptr>::SetSize"); }

    printf("[RtPort]   AddController(6)[shape]: %d pins, %d failed\n",
                c.checked, c.failed);
}

void VerifyCtorShape(RtDiffSite* site) {
    RtDiffRegister(site);
    const uint8_t* body = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x180114870));
    if (!body) return;
    PinCtx c = { body, 0x1e3, site, 0, 0 };

    // `MOV qword ptr [RSI+0x10],RBX` -> _ui.
    // `48 89 5e ..` is `MOV [RSI+disp8],RBX` and the ctor has SEVEN of them
    // (+0x10, +0x28, +0x38, +0x40, +0x68, +0x70, +0x78), so the pattern is
    // anchored on the vtable stamp `MOV qword ptr [RSI],RAX` in front of it.
    { static const uint8_t p[] = { 0x48, 0x89, 0x06, 0x48, 0x89, 0x5e }; PinDisp(c, p, 6, 1,
        offsetof(DialogInstance, _ui), "DialogInstance::_ui"); }
    // `MOV EAX,dword ptr [RBP+0x40]` -> Dialog::_defaultFlags
    { static const uint8_t p[] = { 0x8b, 0x45 }; PinDisp(c, p, 2, 1,
        offsetof(Dialog, _defaultFlags), "Dialog::_defaultFlags"); }
    // `MOV RCX,qword ptr [RBP+0x38]` -> Dialog::_rootDescription.
    // `48 8b 4d ..` is ALSO `MOV RCX,[RBP+0x20]` further down, so the pattern is
    // anchored on the `MOV dword ptr [RSI+0x48],EBX` that precedes it.
    { static const uint8_t p[] = { 0x89, 0x5e, 0x48, 0x48, 0x8b, 0x4d }; PinDisp(c, p, 6, 1,
        offsetof(Dialog, _rootDescription), "Dialog::_rootDescription"); }
    // `LEA RDX,[RSI+0x50]` -> DialogInstance::_root as the hidden ret slot
    { static const uint8_t p[] = { 0x48, 0x8d, 0x56 }; PinDisp(c, p, 3, 1,
        offsetof(DialogInstance, _root), "DialogInstance::_root"); }
    // `MOV qword ptr [RSI+0x58],RCX` -> DialogInstance::_spriteSheet, the sink
    // of the `MOV RCX,[RBP+0x20]` load
    { static const uint8_t p[] = { 0x48, 0x89, 0x4e }; PinDisp(c, p, 3, 1,
        offsetof(DialogInstance, _spriteSheet), "DialogInstance::_spriteSheet"); }
    // `MOV qword ptr [RSI+0x90],-1` -> the qword -1 at +0x90
    { static const uint8_t p[] = { 0x48, 0xc7, 0x86 }; PinDisp(c, p, 3, 4,
        offsetof(DialogInstance, _timerHandle), "DialogInstance::_timerHandle"); }
    // `MOV qword ptr [RAX+0x90],RSI` -> Element::_dialogParent
    { static const uint8_t p[] = { 0x48, 0x89, 0xb0 }; PinDisp(c, p, 3, 4,
        static_cast<int64_t>(EL::kDialogParent), "Element::_dialogParent"); }
    // `MOV EDX,0x18` / `LEA R8D,[RDX-0x10]` -> Allocate(size 0x18, align 8).
    // PRESENCE-only: a bare `BA` byte is everywhere, so the size and the align
    // both live inside the pattern.
    { static const uint8_t p[] = { 0xba, 0x18, 0x00, 0x00, 0x00, 0x44, 0x8d, 0x42, 0xf0 };
      PinPresent(c, p, 9, "LinkedList node MemAlloc(0x18, align 8)"); }
    // `CMP dword ptr [RBP+0x50],0x0` -> Dialog::_controllerCount
    { static const uint8_t p[] = { 0x83, 0x7d }; PinDisp(c, p, 2, 1,
        offsetof(Dialog, _controllerCount), "Dialog::_controllerCount"); }
    // `ADD RDI,0x20` -> the controller-config entry stride
    { static const uint8_t p[] = { 0x48, 0x83, 0xc7 }; PinDisp(c, p, 3, 1,
        static_cast<int64_t>(sizeof(RtUI::DialogControllerConfigEntry)),
        "DialogControllerConfigEntry stride"); }

    // ---- THE ARGUMENT MAPPING THAT THIS PORT GOT WRONG ONCE ----------------
    // These three pins exist because the field NAMES in Dialog.h were the
    // opposite of the parameters they feed, and the first draft of the loop
    // above trusted the names. Lane DLG2 has since fixed the header, so the
    // names and the offsets now agree — and these pins are what will catch it
    // if anyone swaps either back. They pin the mapping to the shipped bytes:
    //   LEA RAX,[R9+0x18]   -> arg 6, the String
    //   MOV RAX,[R9+0x08]   -> arg 5, ExternalPtr<ElementDescription>
    //   MOV R9,[R9+0x10]    -> arg 4, ExternalPtr<DialogControllerConfig>
    { static const uint8_t p[] = { 0x49, 0x8d, 0x41 }; PinDisp(c, p, 3, 1,
        offsetof(RtUI::DialogControllerConfigEntry, _name),
        "config entry +0x18 -> arg6 (String)"); }
    { static const uint8_t p[] = { 0x49, 0x8b, 0x41 }; PinDisp(c, p, 3, 1,
        offsetof(RtUI::DialogControllerConfigEntry, _elementDesc),
        "config entry +0x08 -> arg5 (ElementDescription)"); }
    { static const uint8_t p[] = { 0x4d, 0x8b, 0x49 }; PinDisp(c, p, 3, 1,
        offsetof(RtUI::DialogControllerConfigEntry, _config),
        "config entry +0x10 -> arg4 (DialogControllerConfig)"); }
    // and the 6-arg AddController it forwards to
    { static const uint8_t p[] = { 0x48, 0x8b, 0xce }; PinCall(c, p, 3,
        0x180114870, 0x180113470, "AddController(6)"); }

    printf("[RtPort]   ??0DialogInstance[shape]: %d pins, %d failed\n",
                c.checked, c.failed);
}

// ---- THE NEGATIVE CONTROL FOR ORACLE (b) -----------------------------------
// Read this line BEFORE the four green ones. A pin that cannot fail is not a
// check, and every one of the pins above would report clean against a body that
// does not contain what it claims — unless the decoder actually discriminates.
// This runs the SAME `PinDisp` against the SAME shipped bytes with (i) a value
// deliberately off by one, (ii) a pattern that is not in the body, and (iii) a
// pattern that occurs many times. All three MUST be rejected.
// (reference_probe_needs_an_effectiveness_control.md,
//  reference_diagnostic_field_cannot_express_the_answer.md)
void VerifyPinControls() {
    const uint8_t* body = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x1801137c0));
    if (!body) {
        printf("[RtPort]   [negative control] SKIPPED -- the shipped StartTimer "
                    "body is not readable, so the four [shape] lines above are "
                    "UNCONTROLLED and prove nothing.\n");
        return;
    }
    static RtDiffSite sink = { "UI3 pin negative control (sink)", 0, 0, false, false };
    RtDiffRegister(&sink);
    PinCtx c = { body, 0x77, &sink, 0, 0 };

    // (i) right pattern, WRONG expected value
    { static const uint8_t p[] = { 0x44, 0x39, 0x51 };
      PinDisp(c, p, 3, 1, 0x71, "control: _timers._size off by one"); }
    const int afterWrongValue = c.failed;
    // (ii) a pattern that is not there
    { static const uint8_t p[] = { 0xde, 0xad, 0xbe, 0xef };
      PinDisp(c, p, 4, 1, 0, "control: absent pattern"); }
    const int afterAbsent = c.failed;
    // (iii) a pattern that occurs many times (a bare REX.W prefix)
    { static const uint8_t p[] = { 0x48 };
      PinDisp(c, p, 1, 1, 0, "control: ambiguous pattern"); }
    const int afterAmbiguous = c.failed;

    const bool ok = (afterWrongValue == 1) && (afterAbsent == 2) && (afterAmbiguous == 3);
    printf("[RtPort]   [negative control] pin decoder %s "
                "(wrong-value caught=%d, absent caught=%d, ambiguous caught=%d of 3)\n",
                ok ? "DISCRIMINATES -- the [shape] lines above mean something"
                   : "*** FAILED TO REJECT *** -- IGNORE the [shape] lines above",
                afterWrongValue, afterAbsent - afterWrongValue,
                afterAmbiguous - afterAbsent);
}

// ============================================================================
// ORACLE (a) — SYNTHETIC-SUBJECT DIFFERENTIAL for ?StartTimer@
//
// Two private subjects, identical byte-for-byte. OURS runs on one, THE SHIPPED
// BODY on the other, and the `_timers` header plus every element is compared.
// The DLL is the authority and it runs for real; nothing live is touched, so
// there is no "our body already did it, so retail's guard took the early-out"
// hole (the shape rt_predict.h was written about).
//
// THE CASE MATRIX drives arms a live boot does not reach:
//   0  first append into an EMPTY array          (data==null, capacity 0)
//   1  second append                             (the capacity*3 growth arm)
//   2  append with an id ALREADY PRESENT          (retail's duplicate append)
//   3  a long run, crossing two growths
//   4  a negative id and a negative time
//
// EXCLUDED FROM THE COMPARE: bytes +0x14..0x17 of every element. Retail builds
// its fill record on the stack with three stores and the 0x18-byte byte copy
// carries whatever the fourth word held; ours does the same. Neither side
// WRITES those bytes, so comparing them fails 100 % of cases and reads as a
// catastrophic defect (reference_oracle_must_not_compare_unwritten_bytes.md).
// They are excluded, counted, and printed — never silently skipped.
// ============================================================================
struct TimerSubject {
    // Only `_timers` is touched by StartTimer, but the whole ctor extent is
    // allocated so a stray write lands inside the subject and is SEEN rather
    // than corrupting the harness.
    uint8_t bytes[RtUI::kDialogInstanceAllocSize];
};

bool CompareTimerArrays(const void* a, const void* b, int* excluded, char* msg, size_t msgN) {
    const Collection::Array<DialogTimer>* pa =
        reinterpret_cast<const Collection::Array<DialogTimer>*>(
            B(a, offsetof(DialogInstance, _timers)));
    const Collection::Array<DialogTimer>* pb =
        reinterpret_cast<const Collection::Array<DialogTimer>*>(
            B(b, offsetof(DialogInstance, _timers)));

    if (pa->_size != pb->_size) {
        std::snprintf(msg, msgN, "size ours=%d theirs=%d", pa->_size, pb->_size);
        return false;
    }
    if (pa->_capacity != pb->_capacity) {
        std::snprintf(msg, msgN, "capacity ours=%d theirs=%d", pa->_capacity, pb->_capacity);
        return false;
    }
    // `_data` is a heap pointer and MUST differ (two separate allocations); a
    // compare of it would fail always and prove nothing. What is compared is
    // whether BOTH are null or BOTH are not.
    if ((pa->_data == nullptr) != (pb->_data == nullptr)) {
        std::snprintf(msg, msgN, "one side allocated and the other did not");
        return false;
    }
    for (int32_t i = 0; i < pa->_size; ++i) {
        const DialogTimer& ea = pa->_data[i];
        const DialogTimer& eb = pb->_data[i];
        *excluded += 4;   // +0x14..0x17, never written by either side
        if (ea._remaining != eb._remaining || ea._period != eb._period ||
            ea._id != eb._id) {
            std::snprintf(msg, msgN,
                          "element %d ours={%lld,%lld,%d} theirs={%lld,%lld,%d}", i,
                          (long long)ea._remaining, (long long)ea._period, ea._id,
                          (long long)eb._remaining, (long long)eb._period, eb._id);
            return false;
        }
    }
    std::snprintf(msg, msgN, "%d elements identical", pa->_size);
    return true;
}

// `corrupt` is the negative control: when set, ours is run through a body with
// one deliberate defect (the id comparison inverted), and the round MUST be
// caught. A probe whose corruption slips through was never measuring anything.
void StartTimerCorrupted(void* self, int32_t id, int64_t time) {
    DialogInstance* const d = static_cast<DialogInstance*>(self);
    for (int32_t i = 0; i < d->_timers._size; ++i) {
        DialogTimer& rec = d->_timers._data[i];
        if (rec._id != id) {           // <-- INVERTED. The defect.
            rec._remaining = time;
            rec._period    = time;
        }
    }
    DialogTimer tmp;
    tmp._remaining = time;
    tmp._period    = time;
    tmp._id        = id;
    ArraySetSize18(&d->_timers, d->_timers._size + 1, &tmp);
}

void RunStartTimerDifferential() {
    using Fn = void (*)(void*, int32_t, int64_t);
    // THE RAW DOOR IS CORRECT HERE AND ONLY HERE: this is the oracle's
    // "theirs", and going through the seam would resolve to OUR OWN body once
    // registration has happened, turning the differential into a tautology
    // (reference_resolveaddr_consults_port_registry_first.md).
    Fn theirs = reinterpret_cast<Fn>(ResolveRuntimeRaw(0x1801137c0));
    if (!theirs) {
        printf("[RtPort]   StartTimer[diff] SKIPPED -- no shipped body to diff "
                    "against; the port is UNVERIFIED, not verified-clean.\n");
        return;
    }

    static RtDiffSite site = { "UI::DialogInstance::StartTimer[synthetic diff]",
                               0, 0, false, false };
    static RtDiffSite ctrl = { "UI::DialogInstance::StartTimer[negative control]",
                               0, 0, false, false };
    RtDiffRegister(&site);
    RtDiffRegister(&ctrl);

    struct Case { int32_t id; int64_t t; const char* what; };
    static const Case kCases[] = {
        { 1,   1000, "first append into an empty array" },
        { 2,   2000, "second append (growth arm)" },
        { 1,   3000, "id ALREADY PRESENT -- retail appends a duplicate anyway" },
        { 3,   4000, "third" },
        { 1,   5000, "the duplicate id hit twice" },
        { 4,   6000, "fourth" },
        { 5,   7000, "fifth" },
        { 6,   8000, "sixth -- crosses the second growth" },
        { -7,    -9, "negative id and negative time" },
    };

    for (int pass = 0; pass < 2; ++pass) {
        const bool control = (pass == 1);
        TimerSubject ours{}, theirsSub{};
        std::memset(&ours, 0, sizeof(ours));
        std::memset(&theirsSub, 0, sizeof(theirsSub));
        int excluded = 0;
        int caught   = 0;
        for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); ++i) {
            if (control) StartTimerCorrupted(&ours, kCases[i].id, kCases[i].t);
            else         RtUiDialogInstance_StartTimer(&ours, kCases[i].id, kCases[i].t);
            theirs(&theirsSub, kCases[i].id, kCases[i].t);

            char msg[256];
            const bool eq = CompareTimerArrays(&ours, &theirsSub, &excluded, msg, sizeof(msg));
            if (!eq) ++caught;
            char oursDesc[320], theirsDesc[320];
            std::snprintf(oursDesc, sizeof(oursDesc), "case %zu (%s): %s",
                          i, kCases[i].what, msg);
            std::snprintf(theirsDesc, sizeof(theirsDesc),
                          "shipped 0x1801137c0 on an identical private subject");
            RtDiffReport(control ? &ctrl : &site, eq, oursDesc, theirsDesc);
        }
        if (!control) {
            printf("[RtPort]   StartTimer[diff]: %zu cases against the shipped body, "
                        "%d caught, %d bytes EXCLUDED (element +0x14..0x17, written by "
                        "neither side)\n",
                        sizeof(kCases) / sizeof(kCases[0]), caught, excluded);
        } else {
            printf("[RtPort]   [negative control] the corrupted StartTimer was caught "
                        "in %d of %zu cases -- %s\n", caught,
                        sizeof(kCases) / sizeof(kCases[0]),
                        caught > 0 ? "the differential DISCRIMINATES"
                                   : "*** IT DOES NOT. IGNORE THE LINE ABOVE ***");
        }
        // Both subjects leak their timer buffers. That is deliberate and
        // bounded (two arrays, ~0x120 bytes each): freeing them would need
        // ~Array, and a probe that also exercises a destructor is testing two
        // things at once.
    }
}

}  // namespace

// ============================================================================
// Registration.
//
// Every mangled name below is repeated AS ITS OWN STRING LITERAL rather than
// through a named constant: tools/runtime_census.py cannot see through a named
// constant and would under-count the lane
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// GREPPED FOR AN EXISTING PORT BEFORE WRITING ONE, by ADDRESS and by MANGLED
// NAME, over code/, engine/, runtime/ and standalone/: none of these four is
// registered anywhere in the tree. A duplicate registration is last-binding-
// wins and would have silently made one body dead code
// (reference_grep_for_the_port_before_writing_it.md).
// ============================================================================
int RtUiDialogCreateRegister() {
    int n = 0;

    RegisterRuntimeExport(
        "??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@PEAVBase@5@PEAXH@Z",
        (void*)&RtUiDialogInstance_Ctor);
    RegisterRuntimeAddr(0x180114870, (void*)&RtUiDialogInstance_Ctor);                ++n;

    RegisterRuntimeExport(
        "?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@"
        "V?$ExternalPtr@VDialogControllerConfig@UI@@@4@"
        "V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z",
        (void*)&RtUiDialogInstance_AddController6);
    RegisterRuntimeAddr(0x180113470, (void*)&RtUiDialogInstance_AddController6);      ++n;

    RegisterRuntimeExport(
        "?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@"
        "V?$ExternalPtr@VDialogControllerConfig@UI@@@4@"
        "V?$InternalPtr@VElement@UI@@@4@@Z",
        (void*)&RtUiDialogInstance_AddControllerTab);
    RegisterRuntimeAddr(0x1801133b0, (void*)&RtUiDialogInstance_AddControllerTab);    ++n;

    RegisterRuntimeExport("?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z",
                          (void*)&RtUiDialogInstance_StartTimer);
    RegisterRuntimeAddr(0x1801137c0, (void*)&RtUiDialogInstance_StartTimer);          ++n;

    // ---- the oracles, run once, here --------------------------------------
    // Each [shape] site reports ONCE. A `calls=N` row here is N PINS, not N
    // invocations of the function — the same note WAVE7_LANE_B.md §4 attaches
    // to its shape checks, repeated because it is misread every time.
    if (RtDiffEnabled()) {
        static RtDiffSite sTimer = { "UI::DialogInstance::StartTimer[shape]",       0, 0, false, false };
        static RtDiffSite sTab   = { "UI::DialogInstance::AddController(tab)[shape]", 0, 0, false, false };
        static RtDiffSite sSix   = { "UI::DialogInstance::AddController(6)[shape]",   0, 0, false, false };
        static RtDiffSite sCtor  = { "UI::DialogInstance::DialogInstance[shape]",     0, 0, false, false };
        VerifyStartTimerShape(&sTimer);
        VerifyAddControllerTabShape(&sTab);
        VerifyAddController6Shape(&sSix);
        VerifyCtorShape(&sCtor);
        VerifyPinControls();
        // ARMED, not run: the differential allocates, and the DLL's allocator
        // has not been STARTED at this point in the boot. See ProbeGate().
        g_probePending = true;
        printf("[RtPort]   StartTimer[diff] ARMED -- it runs on the first call "
                    "into one of these four ports, not here.\n");
    }

    return n;
}
