// ============================================================================
// runtime/src/ui/dialog_ctor_dtor.cpp — Wave 10 lane DLG2
//   `??0Dialog@UI@@QEAA@XZ`  0x180112db0  — the default UI::Dialog constructor
//   `??1Dialog@UI@@UEAA@XZ`  0x180112e40  — the virtual UI::Dialog destructor
// ============================================================================
//
// WHY THESE TWO ROWS EXIST AT ALL, AND WHY THAT IS PROGRESS
// ---------------------------------------------------------------------------
// Neither name was in the UI census before 2026-09-08. They ENTERED it as the
// direct consequence of lane UI3 closing the `UI::DialogInstance` family:
// retail INLINES both bodies into the 6-argument
// `?AddController@DialogInstance@UI@@` (its stack temporary is a `UI::Dialog`,
// proved by the `??_7Dialog@UI@@6B@` stamp at 0x180112deb), and UI3's port calls
// the out-of-line exports by name instead of re-inlining them.
//
// So the port replaced an INLINED, UNNAMED dependency with a NAMED, SCOPEABLE
// export. That is `reference_porting_can_grow_the_cutover_surface.md` working
// as intended, not a regression: the two bodies always ran on this path, and
// the census could not see them only because nothing in our tree named them.
// This file closes both.
//
// ---------------------------------------------------------------------------
// THE TWO MANGLINGS, READ OUT OF THE SHIPPED EXPORT DIRECTORY FIRST
// ---------------------------------------------------------------------------
// `tools/pe_exports.py` against C:\GOG Games\Banished\Runtime-x64.dll
// (4,926 exports), BEFORE a line of body was written — a name the DLL does not
// contain resolves to NULL in silence, the port still registers, and the census
// still counts it (reference_resolveexport_phantom_names.md):
//
//   0x180112db0  ??0Dialog@UI@@QEAA@XZ    present
//   0x180112e40  ??1Dialog@UI@@UEAA@XZ    present
//
// Both at exactly the addresses the disassembly gives. NO PHANTOM.
//
// *** THE TWO SPELLINGS DIFFER AND BOTH ARE LOAD-BEARING. *** The ctor is
// `QEAA` (public, non-virtual). The dtor is `UEAA` — the VIRTUAL spelling.
// `??1Dialog@UI@@QEAA@XZ` is NOT in the export table and would resolve NULL
// without a word of complaint (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md,
// which is exactly this defect one class over).
//
// The three vtables the ctor stamps were checked the same way and are exported
// at exactly the addresses the shipped `LEA`s name:
//   ??_7Base@Object@@6B@      0x180154788
//   ??_7External@Object@@6B@  0x1801547a0
//   ??_7Dialog@UI@@6B@        0x1801562a0
// They are read through `ResolveExport` rather than minted. A ported ctor that
// mints its own vtable puts a SECOND vtable in play and the object then fails
// every `IsDerivedFrom` the DLL performs on it — the `??_7Geometry@Graphics@@6B@`
// lever this project has already paid for once.
//
// ---------------------------------------------------------------------------
// `??0Dialog@UI@@QEAA@XZ` — the shipped body, 0x2b instructions
// ---------------------------------------------------------------------------
//   180112dc1  LEA RAX,[0x180154788] ; MOV [RCX],RAX      <- Base vtable
//   180112dcb  LEA RAX,[0x1801547a0] ; MOV [RCX],RAX      <- External vtable
//   180112dd5  LEA RAX,[RCX+0x8]     ; MOV [RSP+0x28],RAX <- SEH-visible temp
//   180112dde  XOR EDX,EDX
//   180112de0  MOV [RAX],RDX                              <- +0x08 THROUGH RAX
//   180112de3  MOV [RCX+0x10],RDX
//   180112de7  MOV [RCX+0x18],RDX
//   180112deb  LEA RAX,[0x1801562a0] ; MOV [RCX],RAX      <- Dialog vtable
//   180112df5  +0x20,+0x28 = 0 (qwords)
//   180112dfd  MOV dword [RCX+0x30],0xffffffff            <- -1, NOT zero
//   180112e04  MOV dword [RCX+0x34],EDX
//   180112e07  MOV qword [RCX+0x38],RDX
//   180112e0b  MOV dword [RCX+0x40],EDX                   <- DWORD; +0x44 UNTOUCHED
//   180112e0e  +0x48 … +0x80 = 0 (qwords, ten of them)
//   180112e31  MOV RAX,RCX ; RET                          <- returns `this`
//
// THREE THINGS ONLY THE DISASSEMBLY SHOWS
// (reference_ghidra_c_output_vs_disassembly.md):
//
//  (a) THE FIRST QWORD IS WRITTEN THREE TIMES. Base, then External, then Dialog
//      — the ordinary MSVC base-ctor chain, fully inlined. The first two stores
//      are dead in the shipped binary, and they are TRANSCRIBED ANYWAY. "It is
//      provably overwritten in this build" is a claim a port should not bake in,
//      and a debugger single-stepping our body should see what it sees in retail.
//
//  (b) `+0x44` IS NEVER WRITTEN. 0x180112e0b is a DWORD store to +0x40 and the
//      next store is a QWORD at +0x48. A "tidy" port that zeroes +0x40 as a
//      qword would write four bytes retail leaves holding whatever the caller's
//      buffer held — and on the 6-arg AddController's path that is UNINITIALISED
//      STACK. The oracle below therefore EXCLUDES +0x44..+0x47 from its compare,
//      counts the exclusion, and prints it. Comparing bytes neither side writes
//      fails 100 % of calls and reads as a catastrophic port defect
//      (reference_oracle_must_not_compare_unwritten_bytes.md).
//
//  (c) `+0x30` IS -1, NOT 0. The single most likely transcription slip in a
//      body that is otherwise 16 zero stores, and it is what negative control
//      (i) below corrupts.
//
// THE CTOR'S WRITTEN EXTENT IS +0x00..+0x87 (the last store is a QWORD at
// +0x80). `runtime/types/ui/Dialog.h` declares out to +0x7f and carries NO
// `sizeof` assert, deliberately. This lane does not invent one either: it adds
// the one member the ctor proves is there (+0x80) and says the size is still
// unknown, because a ctor's written extent is a LOWER BOUND on a class size,
// never the size itself.
//
// ---------------------------------------------------------------------------
// `??1Dialog@UI@@UEAA@XZ` — the shipped body
// ---------------------------------------------------------------------------
//   180112e56  LEA RAX,[0x1801562a0] ; MOV [RCX],RAX      <- re-stamp Dialog vtbl
//   180112e60  ADD RCX,0x78 ; CALL 0x180009460            <- ~Array on +0x78
//   180112e6a  LEA RCX,[RBX+0x68] ; CALL 0x180039440      <- ~Array on +0x68
//   180112e74  LEA RCX,[RBX+0x58] ; CALL 0x180039360      <- ~Array on +0x58
//   180112e7e  LEA RCX,[RBX+0x48] ; CALL 0x180039280      <- ~Array on +0x48
//   180112e88  the System::String release at +0x28 (see below)
//   180112eb2  MOV RCX,RBX ; JMP 0x1800bc950              <- TAIL-JUMP ~External
//
// THE STRING RELEASE IS RETAIL'S ASYMMETRIC ONE, transcribed rather than
// tidied. `_spriteName` at +0x28 is a refcounted `char*` whose count sits at
// `data - 4`:
//   * count reaches zero -> the FIELD is first rewritten to `data - 8`
//     (`ADD qword [RBX+0x28],-0x8`) and THAT is what is handed to `MemFree`.
//     The field is LEFT holding `data - 8`; it is not nulled.
//   * count is still positive -> the field IS nulled.
// The two branches leave the field in different states and that difference is
// observable. `??1DialogInstance@` and `?CreateInstance@ElementDescription@`
// both already reproduce it; this is the third instance of the same shape.
//
// ---------------------------------------------------------------------------
// EVERY CALLEE, AND ITS DISPOSITION — the honest version
// ---------------------------------------------------------------------------
//   0x180009460  ~Array  (+0x78)   PORTED  J1_ENTRY, array.cpp (lane UIC)
//   0x180039440  ~Array  (+0x68)   PORTED  J1_ENTRY, array.cpp (lane UIC)
//   0x180039360  ~Array  (+0x58)   *** NOT PORTED — DLL, by address ***
//   0x180039280  ~Array  (+0x48)   *** NOT PORTED — DLL, by address ***
//   0x1800c7ad0  ?MemFree@         PORTED  (Wave 2 / lane ALLOC), by address
//   0x1800bc950  ~External         PORTED  (external.cpp), by address
//
// THE TWO UNPORTED ONES ARE STATED, NOT GLOSSED. WAVE10_LANE_UI3.md §7b called
// all four "the same family" and suggested two more `J1_ENTRY` lines. **THAT IS
// WRONG AND THIS LANE MEASURED IT RATHER THAN INHERITING IT**
// (reference_holdback_blockers_are_stale_check_them_first.md). All 115 leading
// bytes of each were read with Ghidra `read_memory` and compared against the
// registered J1 reference 0x180033960:
//
//   0x180009460 / 0x180039440   J1 shape: `MOV RDI,RCX ; MOV RSI,[RCX] ; TEST`
//   0x180039360 / 0x180039280   `MOV RDI,RCX ; MOV R8D,[RCX+8] ; MOV RDX,[RCX]
//                                ; CALL rel32` — an EXTRA element-destroy pass
//                                before the free (targets 0x180039641 and
//                                0x180039581 respectively, two DIFFERENT helpers)
//
// They are `~Array<T>` for a T that HAS a destructor. Registering them as J1
// would have silently replaced a body that destroys every element with one that
// only frees the block — a leak, landed under a comment claiming a byte-level
// identity that does not hold. They are called BY ADDRESS through
// `ResolveAddr(Module::Runtime, …)`, which reaches the DLL today and our port
// the moment someone lands one, and they are recorded in the lane doc as this
// port's OWED cutover residue.
//
// Neither is exported (checked against all 4,926 export names), so the address
// door here is a genuine unexported internal and not the "the comment said
// unexported and it was merely private" mistake
// (reference_aeaa_is_private_not_unexported.md).
//
// ---------------------------------------------------------------------------
// THE ALLOCATOR, AND WHY IT IS NOT A BLOCKER
// ---------------------------------------------------------------------------
// The dtor frees. `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` is reached BY
// ADDRESS (0x1800c7ad0), which is the discipline
// `runtime/src/collection/array.cpp` set three waves ago: that address is the
// DLL's allocator when `BANISHED_RT_ALLOC` is unset and OUR ported allocator
// when it is armed, and in the armed case lane ALLOC's swap has patched the
// DLL's own `Allocate`/`Free` prologues so the DLL's internal frees follow us
// too. THERE IS NO CONFIGURATION IN WHICH THE TWO SIDES SEE DIFFERENT HEAPS.
// Nothing in this file reads `BANISHED_RT_ALLOC`; both settings were
// nonetheless run, because "structurally fine" is a prediction.
//
// ---------------------------------------------------------------------------
// VERIFICATION — a SYNTHETIC-SUBJECT DIFFERENTIAL, not a self-check
// ---------------------------------------------------------------------------
// Both bodies are pure functions of one buffer, so they do not need a live
// caller to be verified: two private subjects are built byte-identical, OURS
// runs on one and THE SHIPPED EXPORT on the other, and the whole 0x88-byte
// extent is compared. The DLL is the authority and it runs for real. This is
// NOT the RT_DIFF_SHADOW family, which takes the port's own verdict and never
// consults the DLL (reference_shadow_marker_is_not_an_oracle.md).
//
// Every round reports an EFFECTIVENESS number — how many bytes the two sides
// actually CHANGED from the poison fill. A comparison with nothing to compare
// reads green (reference_probe_needs_an_effectiveness_control.md).
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "../../funcs/rt_diff.h"
#include "../../funcs/rt_port.h"
#include "../../types/ui/Dialog.h"

// ---- ResolveExport / ResolveAddr / ResolveRuntimeRaw -----------------------
// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: resolve.h drags in <windows.h>. These declarations
// must stay byte-identical to resolve.h's; the linker checks them.
void* ResolveExport(const char* mangled_name);
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// The RAW door — the shipped DLL and nothing else. The oracle MUST reach the
// shipped body and not our own prologue, which is what
// `ResolveAddr(Module::Runtime, …)` would hand back once we register
// (reference_resolveaddr_consults_port_registry_first.md), and which is how
// RT_ORIGINAL_ONCE recursed into a stack overflow once already.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

// ============================================================================
// THE WRITTEN EXTENT
// ============================================================================
// The ctor's last store is a QWORD at +0x80, so it writes +0x00..+0x87. This is
// a LOWER BOUND on `sizeof(UI::Dialog)`, never the size; Dialog.h still carries
// no `sizeof` assert and this lane does not add one.
constexpr size_t kDialogCtorExtent = 0x88;

// The one four-byte hole the ctor leaves. 0x180112e0b is `MOV dword [RCX+0x40]`
// and the next store is a QWORD at +0x48, so +0x44..+0x47 keep whatever the
// caller's buffer held. Excluded from the compare, counted, and printed.
constexpr size_t kHoleOff = 0x44;
constexpr size_t kHoleLen = 4;

// ---- The three vtables -----------------------------------------------------
// Resolved by NAME from the DLL's own export table. Never minted: a second
// vtable for a class the DLL also constructs breaks every `IsDerivedFrom` the
// DLL runs on the object.
void* VtBase() {
    static void* v = ResolveExport("??_7Base@Object@@6B@");        // 0x180154788
    return v;
}
void* VtExternal() {
    static void* v = ResolveExport("??_7External@Object@@6B@");    // 0x1801547a0
    return v;
}
void* VtDialog() {
    static void* v = ResolveExport("??_7Dialog@UI@@6B@");          // 0x1801562a0
    return v;
}

// ---- The allocator boundary ------------------------------------------------
// BY ADDRESS, for the reason the banner gives. Identical to
// runtime/src/collection/array.cpp's, including the retry-a-miss contract: a
// caller that ran before the allocator's arming path supplied this address
// would otherwise freeze a null into the static for the life of the process.
void MemFreeStatic(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn) fn(p);
}

// ---- The four `~Array` bodies the dtor calls, in the shipped ORDER ---------
// Order is load-bearing and it is DESCENDING: +0x78, +0x68, +0x58, +0x48.
// Reached through `ResolveAddr(Module::Runtime, …)`, which consults the port
// registry FIRST — so the first two land on array.cpp's J1 port today and the
// last two on the DLL, and all four follow the tree without this file changing.
// NOTE THE SIGNATURE: this takes the ALREADY-RESOLVED pointer. It does NOT
// take the address and resolve it, because that would hide four
// `ResolveAddr(Module::Runtime, 0x…)` literals inside one helper where the
// census cannot see them — see the call sites.
void ArrayDtorAt(uintptr_t resolved, void* member) {
    using Fn = void (*)(void*);
    Fn fn = reinterpret_cast<Fn>(resolved);
    if (fn) fn(member);
}

// ---- `??1External@Object@@UEAA@XZ` 0x1800bc950 -----------------------------
// Retail TAIL-JUMPS here, so ~External's return value is ~Dialog's. It is
// EXPORTED, but it is reached BY ADDRESS on purpose: external.cpp registers the
// port under the ADDRESS door, and going through the address is what makes our
// dtor use our ~External rather than the DLL's.
void* ExternalDtor(void* self) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800bc950));
    return fn ? fn(self) : self;
}

// ---- System::String refcount, at `data - 4` --------------------------------
inline int32_t& StringRefCount(char* data) {
    return *reinterpret_cast<int32_t*>(data - 4);
}

template <typename T>
inline T& F(void* base, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off);
}

// ============================================================================
// NEGATIVE-CONTROL SELECTOR
// ============================================================================
// `BANISHED_DLG2_NEGCTRL=<n>` runs the port with one deliberate defect so the
// oracle has something it MUST catch. A probe whose corruption slips through
// was never measuring anything — and a control that corrupts a value nothing
// compares reports NOT CAUGHT and indicts a sound instrument
// (reference_control_must_pick_a_compared_subject.md), so every one of these
// picks a byte the compare above demonstrably reads.
//
//   1  ctor: `+0x30 = 0` instead of -1        (fact (c) above)
//   2  ctor: zero `+0x40` as a QWORD          (writes into the +0x44 HOLE)
//   3  dtor: null the String field in BOTH branches (retail's asymmetry lost)
//   4  ctor: stamp only the Dialog vtable     (drop the two dead base stamps)
//
// Control 2 is the interesting one and it is deliberately included: the hole is
// EXCLUDED from the compare, so control 2 must be caught by the SEPARATE
// hole-integrity check, not by the byte compare. Scoring them together would
// let one screen's blindness hide inside the other's catch
// (reference_union_control_hides_a_blind_screen.md).
int NegCtrl() {
    static int v = -1;
    if (v < 0) {
        const char* s = std::getenv("BANISHED_DLG2_NEGCTRL");
        v = (s && *s) ? (*s - '0') : 0;
        if (v < 0 || v > 9) v = 0;
    }
    return v;
}

// ============================================================================
// `??0Dialog@UI@@QEAA@XZ`  0x180112db0
// ============================================================================
// Transcribed store for store, in the shipped ORDER, including the two dead
// base-vtable stamps. The `LEA RAX,[RCX+8]` / `MOV [RAX],RDX` pair is written
// through the same intermediate pointer retail uses; it is the SEH frame's
// visible temporary and it costs nothing to keep.
void* DialogCtorBody(void* self) {
    uint8_t* p = static_cast<uint8_t*>(self);
    const int ctl = NegCtrl();

    if (ctl != 4) {
        F<void*>(p, 0x00) = VtBase();       // 180112dc1  dead in this build
        F<void*>(p, 0x00) = VtExternal();   // 180112dcb  dead in this build
    }

    uint64_t* pEight = reinterpret_cast<uint64_t*>(p + 0x08);   // 180112dd5 LEA
    const uint64_t z = 0;                                       // 180112dde XOR EDX,EDX
    *pEight            = z;                 // 180112de0  through RAX
    F<uint64_t>(p, 0x10) = z;               // 180112de3
    F<uint64_t>(p, 0x18) = z;               // 180112de7

    F<void*>(p, 0x00) = VtDialog();         // 180112deb

    F<uint64_t>(p, 0x20) = z;               // 180112df5
    F<uint64_t>(p, 0x28) = z;               // 180112df9
    F<int32_t>(p, 0x30)  = -1;              // 180112dfd  *** -1, NOT 0 ***
    if (ctl == 1) F<int32_t>(p, 0x30) = 0;  //            negative control (i)
    F<int32_t>(p, 0x34)  = 0;               // 180112e04
    F<uint64_t>(p, 0x38) = z;               // 180112e07

    if (ctl == 2) {
        F<uint64_t>(p, 0x40) = z;           //            negative control (ii)
    } else {
        F<int32_t>(p, 0x40) = 0;            // 180112e0b  DWORD; +0x44 UNTOUCHED
    }

    F<uint64_t>(p, 0x48) = z;               // 180112e0e
    F<uint64_t>(p, 0x50) = z;               // 180112e12
    F<uint64_t>(p, 0x58) = z;               // 180112e16
    F<uint64_t>(p, 0x60) = z;               // 180112e1a
    F<uint64_t>(p, 0x68) = z;               // 180112e1e
    F<uint64_t>(p, 0x70) = z;               // 180112e22
    F<uint64_t>(p, 0x78) = z;               // 180112e26
    F<uint64_t>(p, 0x80) = z;               // 180112e2a

    return self;                            // 180112e31  MOV RAX,RCX
}

// ============================================================================
// `??1Dialog@UI@@UEAA@XZ`  0x180112e40
// ============================================================================
void* DialogDtorBody(void* self) {
    uint8_t* p = static_cast<uint8_t*>(self);
    const int ctl = NegCtrl();

    F<void*>(p, 0x00) = VtDialog();                         // 180112e56

    // *** THE FOUR ADDRESSES ARE SPELLED AS LITERALS AT FOUR SEPARATE
    //     `ResolveAddr` CALL SITES, ON PURPOSE. *** The first draft routed all
    //     four through one `ArrayDtorAt(addr, member)` helper, which is tidier
    //     and INVISIBLE to `tools/runtime_census.py`: its scanner greps for a
    //     LITERAL `ResolveAddr(Module::Runtime, 0x…)` and cannot see the same
    //     fact spelled through a variable, so it would have reported this port
    //     as reaching NOTHING unported while it reached two unported bodies
    //     every call (reference_literal_grep_blind_to_variables.md — the fourth
    //     time this project has paid for that blindness, and the address-door
    //     census committed it WHILE measuring it).
    //
    //     Rewriting them this way is what makes the census's own set-diff show
    //     0x180039360 and 0x180039280 as this port's owed residue. That is a
    //     row ENTERING, and it is progress: it was always there and nothing
    //     could see it.
    ArrayDtorAt(ResolveAddr(Module::Runtime, 0x180009460), p + 0x78);  // 180112e64  PORTED (array.cpp J1)
    ArrayDtorAt(ResolveAddr(Module::Runtime, 0x180039440), p + 0x68);  // 180112e6e  PORTED (array.cpp J1)
    ArrayDtorAt(ResolveAddr(Module::Runtime, 0x180039360), p + 0x58);  // 180112e78  NOT PORTED -> DLL
    ArrayDtorAt(ResolveAddr(Module::Runtime, 0x180039280), p + 0x48);  // 180112e82  NOT PORTED -> DLL

    // ---- the System::String release at +0x28, ASYMMETRY AND ALL ------------
    char* s = F<char*>(p, 0x28);                            // 180112e88
    if (s) {                                                // 180112e8f JZ
        --StringRefCount(s);                                // 180112e91 DEC [RAX-4]
        if (StringRefCount(s) == 0) {                       // 180112e94 CMP / JNZ
            if (ctl == 3) {
                F<char*>(p, 0x28) = nullptr;                //   negative control (iii)
                MemFreeStatic(s - 8);
            } else {
                // Retail rewrites the FIELD to `data - 8` and frees THAT; the
                // field is LEFT holding it. It is not nulled on this branch.
                F<char*>(p, 0x28) = s - 8;                  // 180112e9a ADD [RBX+0x28],-8
                MemFreeStatic(F<char*>(p, 0x28));           // 180112e9f / 180112ea3
            }
        } else {
            F<char*>(p, 0x28) = nullptr;                    // 180112eaa
        }
    }

    return ExternalDtor(p);                                 // 180112eba TAIL-JMP
}

// ============================================================================
// THE REGISTERED ENTRY POINTS
// ============================================================================
bool g_probePending = false;
void RunProbe();

inline void ProbeGate() {
    if (!g_probePending) return;
    g_probePending = false;     // clear FIRST — the probe calls back in here
    RunProbe();
}

}  // namespace

// LIVENESS, said out loud. A verified body that nothing calls is dead code that
// reads exactly like a landed port (reference_uncalled_port_is_not_verified.md),
// and the differential below deliberately does NOT need a live caller — so
// nothing else in this file can tell the two apart. These two lines can.
extern "C" void* RtUiDialog_Ctor(void* self) {
    static bool first = true;
    if (first) { first = false; printf("[DLG2] FIRST LIVE CALL into ??0Dialog@UI@@\n"); }
    ProbeGate();
    return DialogCtorBody(self);
}

extern "C" void* RtUiDialog_Dtor(void* self) {
    static bool first = true;
    if (first) { first = false; printf("[DLG2] FIRST LIVE CALL into ??1Dialog@UI@@\n"); }
    ProbeGate();
    return DialogDtorBody(self);
}

// ---------------------------------------------------------------------------
// THE PROBE NEEDS A ROUTE, AND THE FIRST RUN PROVED IT DID NOT HAVE ONE.
// ---------------------------------------------------------------------------
// The gate above fires on the first call into either port. On the first
// measured boot it NEVER FIRED: `??0Dialog` is only reached from the 6-arg
// `?AddController@DialogInstance@` and only on the branch where the controller
// entry carries a NON-NULL ElementDescription (dialog_create.cpp:646, inside
// `if (desc)`), and that branch was not taken in a 20-second autostart boot.
//
// A probe with no route is exactly the `calls=0` reading this project has been
// bitten by four different ways — the body would have shipped with a green pin
// line and a differential that had never executed
// (reference_uncalled_port_is_not_verified.md,
//  reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
//
// BOTH BODIES ARE PURE FUNCTIONS OF ONE BUFFER, so they do not need a live
// caller to be verified — the same argument
// `reference_pure_functions_verify_by_sweep.md` makes for `GetAlignedPosition`,
// which reported calls=0 in a 16.9M-call run and was verified by a sweep
// instead. This entry point lets a caller that DOES run drive the differential.
// `runtime/src/ui/dialog_create.cpp` calls it from its own `ProbeGate`, which
// the first run showed firing (its StartTimer synthetic diff produced results),
// and which by construction runs after the DLL's allocator is up.
extern "C" void RtUiDialogCtorDtorProbeNow() {
    ProbeGate();
}

namespace {

// ============================================================================
// ORACLE (a) — SYNTHETIC-SUBJECT DIFFERENTIAL, both bodies
// ============================================================================
// Two private subjects, filled byte-identically with a poison pattern. OURS
// runs on one, THE SHIPPED EXPORT on the other, and the whole 0x88-byte ctor
// extent is compared with the +0x44 hole excluded.
//
// Nothing live is touched, so there is no "our body already did it, so retail
// took the early-out" hole — the shape rt_predict.h was written about.
//
// The DTOR rounds drive arms a live boot does not reach: the four arrays are
// given REAL heap blocks so the four `~Array` calls have something to free, and
// the String is run through BOTH refcount branches.
constexpr size_t kSubjectSize = 0x100;      // > the ctor extent, so a stray
                                            // write lands INSIDE the subject

struct Subject {
    uint8_t bytes[kSubjectSize];
};

// A recognisable, non-zero, non-(-1) fill. Poison matters: a fill of zero would
// make sixteen of the ctor's seventeen stores invisible and the effectiveness
// count would collapse to one.
void Poison(Subject& s, uint8_t seed) {
    for (size_t i = 0; i < kSubjectSize; ++i)
        s.bytes[i] = static_cast<uint8_t>(0xA5 ^ (i * 7) ^ seed);
}

// How many bytes of the COMPARED extent either side actually changed from the
// poison. This is the effectiveness number: a round where it is 0 proves
// nothing no matter how green the compare is.
int ChangedFromPoison(const Subject& after, const Subject& before) {
    int n = 0;
    for (size_t i = 0; i < kDialogCtorExtent; ++i) {
        if (i >= kHoleOff && i < kHoleOff + kHoleLen) continue;
        if (after.bytes[i] != before.bytes[i]) ++n;
    }
    return n;
}

bool CompareExtent(const Subject& ours, const Subject& theirs,
                   int* excluded, char* msg, size_t msgN) {
    for (size_t i = 0; i < kDialogCtorExtent; ++i) {
        if (i >= kHoleOff && i < kHoleOff + kHoleLen) { ++*excluded; continue; }
        if (ours.bytes[i] != theirs.bytes[i]) {
            std::snprintf(msg, msgN, "byte +0x%02x ours=0x%02x theirs=0x%02x",
                          (unsigned)i, ours.bytes[i], theirs.bytes[i]);
            return false;
        }
    }
    std::snprintf(msg, msgN, "0x%02x bytes identical", (unsigned)kDialogCtorExtent);
    return true;
}

using DialogFn = void* (*)(void*);

DialogFn ShippedCtor() {
    return reinterpret_cast<DialogFn>(ResolveRuntimeRaw(0x180112db0));
}
DialogFn ShippedDtor() {
    return reinterpret_cast<DialogFn>(ResolveRuntimeRaw(0x180112e40));
}

// ---------------------------------------------------------------------------
// A refcounted System::String, built the way retail's allocator lays one out:
// a block whose payload starts 8 bytes in, with the refcount at payload-4.
// `MemAlloc` is reached by address for the same reason `MemFree` is.
// ---------------------------------------------------------------------------
char* MakeString(const char* text, int32_t refs) {
    using Fn = void* (*)(uint64_t, int);
    static Fn alloc = nullptr;
    if (!alloc) alloc = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    if (!alloc) return nullptr;
    const size_t n = std::strlen(text) + 1;
    uint8_t* blk = static_cast<uint8_t*>(alloc(8 + n, 8));
    if (!blk) return nullptr;
    char* payload = reinterpret_cast<char*>(blk + 8);
    *reinterpret_cast<int32_t*>(payload - 4) = refs;
    std::memcpy(payload, text, n);
    return payload;
}

// A live `Collection::Array` header: {data, size, capacity}. Given real data so
// the four `~Array` calls in the dtor have something to free — with an empty
// array all four are a branch not taken and the dtor round proves far less.
void MakeArray(Subject& s, size_t off, int32_t size, int32_t cap, size_t stride) {
    using Fn = void* (*)(uint64_t, int);
    static Fn alloc = nullptr;
    if (!alloc) alloc = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    void* data = (alloc && cap > 0) ? alloc(cap * stride, 8) : nullptr;
    if (data) std::memset(data, 0, cap * stride);
    F<void*>(s.bytes, off + 0x00) = data;
    F<int32_t>(s.bytes, off + 0x08) = size;
    F<int32_t>(s.bytes, off + 0x0c) = cap;
}

// ---------------------------------------------------------------------------
// THE CASE MATRIX
// ---------------------------------------------------------------------------
//  CTOR
//   0  poison seed 0x00
//   1  poison seed 0x5a  — a different fill, so a body that happened to write
//                          the poison value would be caught
//   2  an already-constructed subject re-constructed in place
//  DTOR   (each preceded by a real ctor on BOTH subjects)
//   3  everything empty — all four arrays null, String null
//   4  four REAL array blocks, String refcount 1  (the FREEING branch)
//   5  four REAL array blocks, String refcount 3  (the SHARING branch)
// ---------------------------------------------------------------------------
void RunProbe() {
    static RtDiffSite sCtor = { "UI::Dialog::Dialog[diff]",  0, 0, false, false };
    static RtDiffSite sDtor = { "UI::Dialog::~Dialog[diff]", 0, 0, false, false };
    static RtDiffSite sHole = { "UI::Dialog::Dialog[+0x44 hole]", 0, 0, false, false };
    RtDiffRegister(&sCtor);
    RtDiffRegister(&sDtor);
    RtDiffRegister(&sHole);

    DialogFn theirCtor = ShippedCtor();
    DialogFn theirDtor = ShippedDtor();
    if (!theirCtor || !theirDtor) {
        printf("[RtPort]   [DLG2 diff] SKIPPED -- the shipped bodies are not "
               "readable, so this port is UNVERIFIED against retail.\n");
        return;
    }

    int rounds = 0, caught = 0, effective = 0, excluded = 0;
    int holeChecked = 0, holeFailed = 0;
    char msg[192];

    for (int c = 0; c <= 5; ++c) {
        Subject a, b, pre;
        const uint8_t seed = (c == 1) ? 0x5a : 0x00;
        Poison(a, seed);
        std::memcpy(&b, &a, sizeof(a));
        std::memcpy(&pre, &a, sizeof(a));

        if (c == 2) { DialogCtorBody(&a); theirCtor(&b); std::memcpy(&pre, &a, sizeof(a)); }

        bool eq;
        if (c <= 2) {
            // ---- CTOR round -------------------------------------------------
            void* ra = DialogCtorBody(&a);
            void* rb = theirCtor(&b);
            ++rounds;
            effective += ChangedFromPoison(a, pre);

            // THE HOLE IS ITS OWN SCREEN, SCORED SEPARATELY. The byte compare
            // is structurally blind to +0x44..+0x47 (it skips them), so folding
            // this into it would let one screen's blindness hide inside the
            // other's catch (reference_union_control_hides_a_blind_screen.md).
            ++holeChecked;
            const bool holeOk =
                std::memcmp(a.bytes + kHoleOff, pre.bytes + kHoleOff, kHoleLen) == 0 &&
                std::memcmp(b.bytes + kHoleOff, pre.bytes + kHoleOff, kHoleLen) == 0;
            if (!holeOk) {
                ++holeFailed;
                char hm[96];
                std::snprintf(hm, sizeof(hm), "case %d: +0x44 was WRITTEN", c);
                RtDiffReport(&sHole, false, hm, "retail leaves +0x44..0x47 untouched");
            } else {
                RtDiffReport(&sHole, true, "untouched", "untouched");
            }

            eq = CompareExtent(a, b, &excluded, msg, sizeof(msg));
            if (eq && ra != &a) { eq = false; std::snprintf(msg, sizeof(msg), "ours returned %p, not this", ra); }
            if (eq && rb != &b) { eq = false; std::snprintf(msg, sizeof(msg), "theirs returned %p, not this", rb); }
            RtDiffReport(&sCtor, eq, msg, eq ? msg : "shipped ??0Dialog@UI@@");
            if (!eq) ++caught;
        } else {
            // ---- DTOR round -------------------------------------------------
            // Construct BOTH with the SHIPPED ctor, so a ctor defect cannot
            // masquerade as a dtor divergence, then diverge the state.
            theirCtor(&a);
            theirCtor(&b);
            if (c >= 4) {
                const int32_t refs = (c == 4) ? 1 : 3;
                MakeArray(a, 0x78, 2, 4, 8);   MakeArray(b, 0x78, 2, 4, 8);
                MakeArray(a, 0x68, 2, 4, 8);   MakeArray(b, 0x68, 2, 4, 8);
                MakeArray(a, 0x58, 2, 4, 0x10); MakeArray(b, 0x58, 2, 4, 0x10);
                MakeArray(a, 0x48, 2, 4, 0x20); MakeArray(b, 0x48, 2, 4, 0x20);
                F<char*>(a.bytes, 0x28) = MakeString("dlg2", refs);
                F<char*>(b.bytes, 0x28) = MakeString("dlg2", refs);
            }
            std::memcpy(&pre, &a, sizeof(a));

            DialogDtorBody(&a);
            theirDtor(&b);
            ++rounds;
            effective += ChangedFromPoison(a, pre);

            // `+0x28` and the four array `_data` slots hold HEAP POINTERS from
            // two separate allocations and MUST differ. What is compared is the
            // STATE they were left in — null vs not-null, and for the freeing
            // branch the `data - 8` rewrite — never the pointer value
            // (reference_oracle_must_not_compare_unwritten_bytes.md is the same
            // lesson from the other side).
            eq = true;
            msg[0] = 0;
            for (size_t off : { (size_t)0x48, (size_t)0x58, (size_t)0x68, (size_t)0x78 }) {
                const bool na = F<void*>(a.bytes, off) == nullptr;
                const bool nb = F<void*>(b.bytes, off) == nullptr;
                if (na != nb) {
                    eq = false;
                    std::snprintf(msg, sizeof(msg),
                                  "array +0x%02x: ours %s, theirs %s", (unsigned)off,
                                  na ? "null" : "non-null", nb ? "null" : "non-null");
                    break;
                }
                if (eq && (F<int32_t>(a.bytes, off + 8) != F<int32_t>(b.bytes, off + 8) ||
                           F<int32_t>(a.bytes, off + 12) != F<int32_t>(b.bytes, off + 12))) {
                    eq = false;
                    std::snprintf(msg, sizeof(msg),
                                  "array +0x%02x header: ours {%d,%d} theirs {%d,%d}",
                                  (unsigned)off,
                                  F<int32_t>(a.bytes, off + 8), F<int32_t>(a.bytes, off + 12),
                                  F<int32_t>(b.bytes, off + 8), F<int32_t>(b.bytes, off + 12));
                    break;
                }
            }
            if (eq) {
                // The String field. Case 4 (refcount 1) must leave BOTH sides
                // holding `payload - 8`; case 5 must leave BOTH null. That
                // asymmetry IS retail's behaviour and it is the thing negative
                // control 3 destroys.
                char* fa = F<char*>(a.bytes, 0x28);
                char* fb = F<char*>(b.bytes, 0x28);
                const bool wantNull = (c != 4);
                if ((fa == nullptr) != (fb == nullptr)) {
                    eq = false;
                    std::snprintf(msg, sizeof(msg), "String +0x28: ours %s, theirs %s",
                                  fa ? "kept" : "nulled", fb ? "kept" : "nulled");
                } else if ((fa == nullptr) != wantNull) {
                    eq = false;
                    std::snprintf(msg, sizeof(msg),
                                  "String +0x28: BOTH %s, retail's %s branch wants %s",
                                  fa ? "kept" : "nulled",
                                  (c == 4) ? "freeing" : "sharing",
                                  wantNull ? "nulled" : "kept");
                }
                if (eq) {
                    // The vtable ~External re-stamps last.
                    if (F<void*>(a.bytes, 0x00) != F<void*>(b.bytes, 0x00)) {
                        eq = false;
                        std::snprintf(msg, sizeof(msg), "vtable +0x00: ours=%p theirs=%p",
                                      F<void*>(a.bytes, 0x00), F<void*>(b.bytes, 0x00));
                    }
                }
                if (eq) std::snprintf(msg, sizeof(msg), "case %d: arrays, String and vtable agree", c);
            }
            RtDiffReport(&sDtor, eq, msg, eq ? msg : "shipped ??1Dialog@UI@@");
            if (!eq) ++caught;
        }
    }

    const int ctl = NegCtrl();
    printf("[RtPort]   [DLG2 diff] rounds=%d divergences=%d  "
           "effectiveness=%d bytes changed (excluded=%d in the +0x44 hole)  "
           "hole screen: %d checked / %d failed  NEGCTRL=%d\n",
           rounds, caught, effective, excluded, holeChecked, holeFailed, ctl);
    if (effective == 0) {
        printf("[RtPort]   [DLG2 diff] *** EFFECTIVENESS ZERO -- neither side "
               "changed a compared byte, so the green compare above proves "
               "NOTHING. ***\n");
    }
    if (ctl != 0 && caught == 0 && holeFailed == 0) {
        printf("[RtPort]   [DLG2 diff] *** NEGCTRL=%d WAS NOT CAUGHT -- the "
               "oracle above is not measuring what it claims. ***\n", ctl);
    }
}

// ============================================================================
// ORACLE (b) — SHIPPED-BYTE PINS, with a decoder control
// ============================================================================
// Every immediate and every structure offset this port compiles in is a literal
// in the shipped instruction stream. These locate each BY PATTERN (never by a
// hardcoded byte offset — that is exactly the input that made uic_bytediff.py's
// first version report a confident wrong mismatch), require the pattern to
// occur EXACTLY ONCE in the body, and compare the decoded value against the
// constant this file actually uses.
//
// This is a PRE-FLIGHT ON THE TRANSCRIPTION, not a substitute for oracle (a):
// it cannot see a control-flow error. `calls=N` on these sites is N PINS, not N
// invocations.
struct PinCtx {
    const uint8_t* body;
    size_t         len;
    RtDiffSite*    site;
    int            checked;
    int            failed;
};

// Find `pat` exactly once, then decode `width` bytes at `pat + patN + skip`
// as a little-endian value and require it to equal `expect`.
void Pin(PinCtx& c, const uint8_t* pat, size_t patN, size_t skip, size_t width,
         int64_t expect, const char* what) {
    ++c.checked;
    int hits = 0; size_t at = 0;
    for (size_t i = 0; i + patN <= c.len; ++i)
        if (std::memcmp(c.body + i, pat, patN) == 0) { ++hits; at = i; }

    char ours[96], theirs[96];
    bool ok = false;
    if (hits != 1) {
        std::snprintf(theirs, sizeof(theirs), "pattern occurs %d times, need exactly 1", hits);
        std::snprintf(ours, sizeof(ours), "%s = %lld", what, (long long)expect);
    } else {
        int64_t v = 0;
        for (size_t k = 0; k < width; ++k)
            v |= (int64_t)c.body[at + patN + skip + k] << (8 * k);
        if (width == 4) v = (int32_t)v;   // sign-extend a disp32/imm32
        ok = (v == expect);
        std::snprintf(ours, sizeof(ours), "%s = %lld", what, (long long)expect);
        std::snprintf(theirs, sizeof(theirs), "shipped +0x%02x = %lld", (unsigned)at, (long long)v);
    }
    if (!ok) ++c.failed;
    RtDiffReport(c.site, ok, ours, theirs);
}

void VerifyShippedPins(RtDiffSite* site) {
    const uint8_t* ctor = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x180112db0));
    const uint8_t* dtor = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x180112e40));
    if (!ctor || !dtor) {
        printf("[RtPort]   [DLG2 pins] SKIPPED -- shipped bodies unreadable; the "
               "transcription is UNPINNED.\n");
        return;
    }
    PinCtx c = { ctor, 0x89, site, 0, 0 };

    // `MOV dword ptr [RCX+0x30],0xffffffff`  ->  C7 41 30 ff ff ff ff
    { static const uint8_t p[] = { 0xC7, 0x41, 0x30 };
      Pin(c, p, 3, 0, 4, -1, "ctor +0x30 immediate"); }
    // `MOV dword ptr [RCX+0x40],EDX`  ->  89 51 40 : a DWORD store, which is
    // the whole of fact (b). If this were `48 89 51 40` the hole would not exist.
    { static const uint8_t p[] = { 0x89, 0x51, 0x40 };
      Pin(c, p, 3, 0, 0, 0, "ctor +0x40 is a DWORD store (89, no REX.W)"); }
    // The last store: `MOV qword ptr [RCX+0x80],RDX` -> 48 89 91 80 00 00 00
    { static const uint8_t p[] = { 0x48, 0x89, 0x91 };
      Pin(c, p, 3, 0, 4, 0x80, "ctor written extent ends at +0x80"); }

    c.body = dtor; c.len = 0x7b;
    // The four array-dtor member offsets, in the shipped descending order.
    { static const uint8_t p[] = { 0x48, 0x83, 0xC1 };          // ADD RCX,0x78
      Pin(c, p, 3, 0, 1, 0x78, "dtor array member 1 = +0x78"); }
    { static const uint8_t p[] = { 0x48, 0x8D, 0x4B, 0x68 };    // LEA RCX,[RBX+0x68]
      Pin(c, p, 4, 0, 0, 0, "dtor array member 2 = +0x68"); }
    { static const uint8_t p[] = { 0x48, 0x8D, 0x4B, 0x58 };
      Pin(c, p, 4, 0, 0, 0, "dtor array member 3 = +0x58"); }
    { static const uint8_t p[] = { 0x48, 0x8D, 0x4B, 0x48 };
      Pin(c, p, 4, 0, 0, 0, "dtor array member 4 = +0x48"); }
    // The String field: `MOV RAX,[RBX+0x28]` -> 48 8B 43 28
    { static const uint8_t p[] = { 0x48, 0x8B, 0x43, 0x28 };
      Pin(c, p, 4, 0, 0, 0, "dtor String field = +0x28"); }
    // The asymmetric rewrite: `ADD qword ptr [RBX+0x28],-0x8`.
    // *** THE FIRST DRAFT OF THIS PIN HAD THE ENCODING WRONG *** and the pin
    // line reported `10 pins, 1 failed` on the first run: it guessed
    // `48 83 6B 28 08`, reading the sign into the immediate and the opcode
    // extension as /5 (SUB). `ADD r/m64, imm8` is REX.W 83 /0, and the modrm
    // for [RBX+disp8] with reg field 0 is 0x43, with the -8 in the immediate:
    //   180112e9a:  48 83 43 28 F8
    // (read back with Ghidra `read_memory`, not derived from the mnemonic).
    // A pin that FAILS is the instrument working — it is the only reason this
    // was caught before the body shipped, and it is why the decoder gets its
    // own negative control below.
    { static const uint8_t p[] = { 0x48, 0x83, 0x43, 0x28, 0xF8 };
      Pin(c, p, 5, 0, 0, 0, "dtor freeing branch rewrites the field to data-8"); }
    // The refcount lives at data-4: `DEC dword ptr [RAX-0x4]` -> FF 48 FC
    { static const uint8_t p[] = { 0xFF, 0x48, 0xFC };
      Pin(c, p, 3, 0, 0, 0, "dtor refcount at data-4"); }

    printf("[RtPort]   ??0/??1Dialog[pins]: %d pins, %d failed\n", c.checked, c.failed);
}

// ---- THE DECODER'S OWN NEGATIVE CONTROL ------------------------------------
// Read this BEFORE the pin line above. Every pin would report clean against a
// body that does not contain what it claims, unless the decoder actually
// discriminates. The SAME `Pin` is re-run against the SAME shipped bytes with
// (i) a right pattern and a wrong expected value, (ii) a pattern that is not
// there, and (iii) a pattern that occurs many times. All three MUST be rejected
// (reference_probe_needs_an_effectiveness_control.md).
void VerifyPinControls() {
    const uint8_t* body = reinterpret_cast<const uint8_t*>(ResolveRuntimeRaw(0x180112db0));
    if (!body) {
        printf("[RtPort]   [DLG2 pin control] SKIPPED -- the pin lines above are "
               "UNCONTROLLED and prove nothing.\n");
        return;
    }
    static RtDiffSite sink = { "DLG2 pin negative control (sink)", 0, 0, false, false };
    RtDiffRegister(&sink);
    PinCtx c = { body, 0x89, &sink, 0, 0 };

    { static const uint8_t p[] = { 0xC7, 0x41, 0x30 };
      Pin(c, p, 3, 0, 4, 0, "control: +0x30 immediate claimed to be 0"); }
    const int afterWrongValue = c.failed;
    { static const uint8_t p[] = { 0xde, 0xad, 0xbe, 0xef };
      Pin(c, p, 4, 0, 0, 0, "control: absent pattern"); }
    const int afterAbsent = c.failed;
    { static const uint8_t p[] = { 0x48 };
      Pin(c, p, 1, 0, 0, 0, "control: ambiguous pattern"); }
    const int afterAmbiguous = c.failed;

    const bool ok = (afterWrongValue == 1) && (afterAbsent == 2) && (afterAmbiguous == 3);
    (void)ok;   // its only reader is the printf below, which -DBANISHED_STRIP_LOGS removes
    printf("[RtPort]   [DLG2 pin control] decoder %s "
           "(wrong-value caught=%d, absent caught=%d, ambiguous caught=%d of 3)\n",
           ok ? "DISCRIMINATES -- the [pins] line above means something"
              : "*** FAILED TO REJECT *** -- IGNORE the [pins] line above",
           afterWrongValue, afterAbsent - afterWrongValue, afterAmbiguous - afterAbsent);
}

}  // namespace

// ============================================================================
// REGISTRATION
// ============================================================================
// Both doors, both rows. `RegisterRuntimeExport` with the mangled name spelled
// as its OWN string literal (tools/runtime_census.py cannot see through a named
// constant and would under-count the lane —
// reference_census_cannot_see_nonliteral_registrations.md), and
// `RegisterRuntimeAddr` for the address, because `engine/funcs/` and lane UI3's
// `ResolveExport("??0Dialog@UI@@QEAA@XZ")` reach these through different doors
// and a body registered under only one of them is dead code under the other
// (reference_count_the_doors_not_the_calls.md).
int RtUiDialogCtorDtorRegister() {
    int n = 0;

    RegisterRuntimeExport("??0Dialog@UI@@QEAA@XZ", (void*)&RtUiDialog_Ctor);
    RegisterRuntimeAddr(0x180112db0, (void*)&RtUiDialog_Ctor);      ++n;

    RegisterRuntimeExport("??1Dialog@UI@@UEAA@XZ", (void*)&RtUiDialog_Dtor);
    RegisterRuntimeAddr(0x180112e40, (void*)&RtUiDialog_Dtor);      ++n;

    if (RtDiffEnabled()) {
        static RtDiffSite sPins = { "UI::Dialog::ctor/dtor[shape]", 0, 0, false, false };
        VerifyShippedPins(&sPins);
        VerifyPinControls();
        // ARMED, not run: the differential ALLOCATES, and the DLL's allocator
        // has not been STARTED at this point in the boot. It fires on the first
        // call into either port — which on this build is `SplashDLT`'s first
        // dialog, long after `Engine::Startup`.
        g_probePending = true;
        printf("[RtPort]   ??0/??1Dialog[diff] ARMED -- it runs on the first "
               "call into either port, not here.\n");
    }

    return n;
}
