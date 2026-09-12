// ============================================================================
// runtime/src/collection/array_destroy.cpp — Wave 9 lane D2
//
// Collection::Array<T>::~Array for NON-trivially-destructible T (lane D's J2
// and J2b), the per-element destroy helpers those bodies call, and
// Collection::Array<T*>::operator= (lane D's J4, the 0x180003970 half).
// ============================================================================
//
// WHAT THIS RETIRES
//
// Lane Q retired 27 addresses for two bodies by proving the 34-strong
// `Collection::Array<T>` cluster was three functions written many times. The
// same proof runs one level deeper here, and comes out better than lane D
// priced it.
//
// Lane D's ranked list says J2 costs "+6 new lane-D rows", because each of its
// six `~Array` bodies opens with a CALL to an unexported per-element destroy
// helper, and J2b needs a seventh. Lane Q then created five more rows for the
// same reason (three of them destroy helpers). Following every one of those to
// its own depth-2 callees gives ELEVEN destroy-helper addresses. Hashing the
// MNEMONIC SEQUENCE of all eleven (byte hashes find nothing — only the
// RIP-relative displacements differ) says they are **THREE BODIES**, and ten of
// the eleven are two of them:
//
//   BODY A  "free the inner Array<T> at element+off, clear its 16-byte header"
//           0x18001ce90   offs {0x100}                stride 0x110
//           0x1800419b0   offs {0x008}                stride 0x018
//           0x180052110   offs {0x030}                stride 0x040
//           0x180074100   offs {0x020}                stride 0x048
//           0x18005bce0   offs {0x038,0x028,0x008}    stride 0x048
//           (and, as the eh_vector form, J2b's three inner Arrays — see below)
//
//   BODY B  "release the System::String* at element+off"
//           0x180011010   offs {0x000}                stride 0x008
//           0x180005930   offs {0x000}                stride 0x010
//           0x180014a40   offs {0x000}                stride 0x048
//           0x180034440   offs {0x010}                stride 0x018
//           0x1800059e0   offs {0x008,0x000}          stride 0x038
//
// The eleventh, 0x18005c550, is a RECURSIVE destroy that calls one of each body
// before freeing; it is written out separately below.
//
// So J2 does not cost six new rows and lane Q's five were three too many. The
// helpers ARE the parameters, and the parameter is a (stride, offset-list)
// pair, exactly as `SetSize`'s parameter turned out to be (stride, align,
// element-assign).
//
// Three of those eleven — 0x180052110, 0x180005930, 0x1800059e0 — are already
// rows in wave9_internals.csv because lane Q reached them by address. They are
// retired here rather than deepened.
//
// CORRECTION TO LANE Q'S ROW TABLE. Lane Q filed 0x180052110 as "destroy-range
// for Audio's 0x40-byte EffectSlot — PORT WITH THE AUDIO HOLDBACKS". It is
// nothing of the sort: it is BODY A with offs {0x30}, stride 0x40, it touches
// no Audio state, calls nothing but the allocator, and is not gated on lane A.
// Its sibling 0x180051650 (the EffectSlot copy-assign) genuinely IS Audio —
// it calls 0x180050d30 — and stays where lane Q put it.
//
// ---------------------------------------------------------------------------
// THE OUTER BODY (J2), read at 0x180010ab0, identical at the other five
// ---------------------------------------------------------------------------
//     MOV RDI,RCX
//     MOV R8D,[RCX+8]        ; count = this->_size
//     MOV RDX,[RCX]          ; base  = this->_data
//     CALL <helper>          ; RCX still = this, and every helper IGNORES it
//     ... then, instruction for instruction, the J1 body lane Q already wrote:
//     MOV RSI,[RDI] / TEST / Enter / Free(_singleton, p) / Leave
//     XOR EAX,EAX / MOV [RDI],RAX / MOV [RDI+8],RAX      ; _size AND _capacity
//
// So J2 == helper + J1. `ArrayDtorImpl` is lane Q's J1 body, reused unchanged;
// there is no second copy of it in this file.
//
// Every helper's first parameter is `ignoredThis`: each one spills RCX to the
// shadow space at entry and never reads it again (verified in all thirteen
// disassemblies — reference_read_the_callee_before_holding_back_on_an_arg.md).
// It is kept in the signature because retail passes it and the registered entry
// points must have retail's arity, not a tidied one.
//
// ---------------------------------------------------------------------------
// J2b (0x18001c8c0) IS BODY A, VIA A CRT TRAMPOLINE
// ---------------------------------------------------------------------------
// Lane D filed 0x18001c8c0 separately because it destroys its elements through
// MSVC's `eh_vector_destructor_iterator` (0x18007ec98) instead of a bespoke
// loop, and priced it as "J1 + CRT iterator + a per-element dtor". Following it
// through says otherwise:
//
//     LEA RCX,[RBX+0x1c0]    ; first inner object
//     LEA R9,[0x18001b960]   ; the per-element dtor
//     MOV EDX,0x10           ; sizeof = 0x10
//     LEA R8D,[RDX-0xd]      ; count  = 3
//     CALL 0x18007ec98
//     ADD RBX,0x1f0          ; element stride
//
//   * 0x18001b960 is NOT A FUNCTION in Ghidra's Runtime program — it is five
//     bytes, `E9 EB 0C 00 00`, an incremental-link thunk to **0x18001c650**.
//   * 0x18001c650, read out of raw memory rather than the function list, is the
//     J1 BODY AGAIN — a twelfth copy of `Array<T>::~Array` with a trivially
//     destructible T, byte-for-byte the shape lane Q proved eleven times.
//   * 0x18007ec98 is 32 instructions and its whole content is
//     `p = base + count*size; while (--count >= 0) { p -= size; dtor(p); }` —
//     i.e. **destroy in REVERSE order**.
//
// Put together: 0x18001c8c0 destroys, per element, three inner `Array<T>` at
// +0x1c0 / +0x1d0 / +0x1e0 in DESCENDING order, stride 0x1f0. That is BODY A
// with `offs = {0x1e0, 0x1d0, 0x1c0}`. No CRT dependency survives, and the
// per-element dtor is not a new row.
//
// (Order is preserved because it reaches the allocator, not because it is
// observable in the object: the three slots do not alias, so a comparator on
// the element buffer cannot see the order. Said plainly here so nobody reads
// the probe's green line as proof that the order is right.)
//
// ---------------------------------------------------------------------------
// BODY B's ASYMMETRY — the one thing in this file a tidy-up would break
// ---------------------------------------------------------------------------
// Read at 0x180034440, confirmed at all five:
//
//     RAX = *(char**)(e+off)
//     if (RAX) {
//         DEC dword [RAX-4]                 ; System::String's refcount
//         CMP dword [RAX-4],0
//         JNZ  ->  MOV [e+off], 0           ; STILL SHARED: slot becomes NULL
//         ADD qword [e+off], -8             ; LAST REF: slot becomes str-8 ...
//         MOV RBP,[e+off]
//         JZ  -> skip                       ; ... flags from the ADD: str==8
//         Enter; Free(_singleton, RBP); Leave
//     }                                     ; and the slot KEEPS str-8
//
// The two arms leave the slot holding DIFFERENT THINGS — NULL when the string
// survives, `str-8` (a dangling, just-freed pointer) when it does not. Retail
// never clears it on the freeing path. That is not a bug we may fix: the object
// is being destroyed and the caller's next act is to free the whole buffer, and
// a helpful `= nullptr` would hide a use-after-free that today is loud.
//
// It is also the ONLY part of either body a comparator can see, which makes it
// the load-bearing check in the probe: getting the asymmetry backwards changes
// the buffer and the probe's negative control does exactly that.
//
// The `JZ` after `ADD [e+off],-8` tests the RESULT OF THE ADD, not the refcount
// — the free is skipped when `str == 8`. Unreachable in practice; reproduced
// because "unreachable" is a claim about callers, not about this function.
// `rtcoll::MemFree` (0x1800c7ad0) re-tests null itself, so handing it the
// possibly-null pointer is retail's shape and not a lost guard.
//
// ---------------------------------------------------------------------------
// 0x180011010 IS BODY B, addressed differently
// ---------------------------------------------------------------------------
// Its mnemonic hash differs from the other four (3000b22572 vs 154da96279)
// because MSVC indexed it — `MOV RAX,[R14+RDI*8]` with an INC/CMP counter —
// instead of bumping a pointer by the stride. Every store, every branch and
// every immediate is otherwise the same, and offs{0} / stride 8 makes the two
// addressings compute identical addresses. Filed as Body B, and the difference
// noted here so the next hash sweep does not re-open it.
//
// ---------------------------------------------------------------------------
// THE ALLOCATOR, AND WHAT THIS FILE DOES NOT TAKE
// ---------------------------------------------------------------------------
// Every free here goes through `rtcoll::MemFree` — lane Q's by-address call to
// the exported 0x1800c7ad0, which IS `if (p) { Enter; Free(_singleton, p);
// Leave; }`, instruction for instruction the tail of each of these loops. This
// file adds no new Runtime address beyond the ones it registers, and it does
// NOT arm the allocator: BANISHED_RT_ALLOC is lane E's and carries a standing
// NO-GO. A heap cannot be half-ours. Lane Q's LANE E DEPENDENCY note in
// array.cpp covers this file too — repointing rtcoll::MemAlloc/MemFree at lane
// E's allocator repoints these bodies with it.
//
// ---------------------------------------------------------------------------
// KILL SWITCHES
// ---------------------------------------------------------------------------
//   BANISHED_RT_COLLECTION=0        the whole Collection lane, Q's and this
//   BANISHED_RT_ARRAY_DESTROY=0     only this file's 18 addresses
//   BANISHED_RT_ARRAY_DESTROY_PROBE=1|neg   the oracle (off by default)
//
// Lane Q's `BANISHED_RT_ARRAY_NONTRIVIAL=0` still halves ITS three, and now
// also matters here: with it off, lane Q's specs keep pointing at 0x180052110,
// 0x1800059e0 and 0x180005930 by address — which this file registers — so the
// two switches are not independent. The bisection order is stated in
// docs/runtime_port/WAVE9_LANE_D2.md.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"
#include "array_internal.h"
#include "array_destroy_internal.h"

// Same local declaration array.cpp carries, and for the same reason: resolve.h
// drags in <windows.h>. The linker checks the two spellings match.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtcoll {

// `DestroySpec` and the two body signatures are in array_destroy_internal.h,
// shared with the probe so it drives THESE bodies and not a copy of them.

// ---- BODY A ---------------------------------------------------------------
// Free the inner Array<T>'s buffer and clear its 16-byte header, per element.
//
// The header clear is `[e+off] = 0` and `[e+off+8] = 0` — one qword each, the
// second covering _size AND _capacity together, exactly as J1 does. It happens
// UNCONDITIONALLY, including when the buffer was already null.
void FreeInnerArraysCore(uint8_t* base, int count, const DestroySpec* s) {
    if (count <= 0) return;                       // TEST R8D,R8D / JLE
    const int64_t n = static_cast<int64_t>(count);  // MOVSXD
    for (int64_t i = 0; i < n; ++i) {
        uint8_t* e = base + i * s->stride;
        for (int k = 0; k < s->nOffs; ++k) {
            uint8_t* slot = e + s->offs[k];
            void* p = *reinterpret_cast<void**>(slot);
            if (p) MemFree(p);                    // Enter; Free(_singleton,p); Leave
            *reinterpret_cast<uint64_t*>(slot + 0) = 0;
            *reinterpret_cast<uint64_t*>(slot + 8) = 0;
        }
    }
}

// ---- BODY B ---------------------------------------------------------------
// Release one System::String reference per (element, offset). See the banner
// for why the two arms leave the slot holding different things.
void ReleaseStringsCore(uint8_t* base, int count, const DestroySpec* s) {
    if (count <= 0) return;
    const int64_t n = static_cast<int64_t>(count);
    for (int64_t i = 0; i < n; ++i) {
        uint8_t* e = base + i * s->stride;
        for (int k = 0; k < s->nOffs; ++k) {
            uint8_t** slot = reinterpret_cast<uint8_t**>(e + s->offs[k]);
            uint8_t* str = *slot;
            if (!str) continue;
            int32_t* rc = reinterpret_cast<int32_t*>(str - 4);
            --*rc;                                // DEC dword [RAX-4]
            if (*rc != 0) {                       // CMP / JNZ — still shared
                *slot = nullptr;
                continue;
            }
            uint8_t* blk = str - 8;               // ADD qword [slot],-8
            *slot = blk;                          // ... and the slot KEEPS it
            if (blk != nullptr) MemFree(blk);     // JZ on the ADD's result
        }
    }
}

// ============================================================================
// The specs, one row per registered helper address.
//
// Every row was read out of that address's own disassembly: `stride` is the
// `ADD RBX,imm` (or the `*8` scale for 0x180011010) at the bottom of the loop,
// and each offset is the `LEA RBX,[RDX+imm]` seed plus the loop's displacement.
// ============================================================================

// ---- BODY A rows ----
constexpr DestroySpec kD_InnerArr_110 = { "inner Array @+0x100, stride 0x110", 0x110, 1, { 0x100 } };
constexpr DestroySpec kD_InnerArr_18  = { "inner Array @+0x008, stride 0x018", 0x018, 1, { 0x008 } };
constexpr DestroySpec kD_InnerArr_40  = { "inner Array @+0x030, stride 0x040", 0x040, 1, { 0x030 } };
constexpr DestroySpec kD_InnerArr_48  = { "inner Array @+0x020, stride 0x048", 0x048, 1, { 0x020 } };
constexpr DestroySpec kD_InnerArr3_48 = { "3 inner Arrays, stride 0x048",      0x048, 3, { 0x038, 0x028, 0x008 } };
// J2b's element: three inner Arrays destroyed in DESCENDING order by
// eh_vector_destructor_iterator. Not a registered helper of its own — it has no
// address, it is the loop 0x18001c8c0 inlines.
constexpr DestroySpec kD_InnerArr3_1f0 = { "3 inner Arrays (eh_vector, reverse), stride 0x1f0",
                                       0x1f0, 3, { 0x1e0, 0x1d0, 0x1c0 } };

// ---- BODY B rows ----
constexpr DestroySpec kD_Str_08 = { "String @+0x000, stride 0x008", 0x008, 1, { 0x000 } };
constexpr DestroySpec kD_Str_10 = { "String @+0x000, stride 0x010", 0x010, 1, { 0x000 } };
constexpr DestroySpec kD_Str_48 = { "String @+0x000, stride 0x048", 0x048, 1, { 0x000 } };
constexpr DestroySpec kD_Str_18 = { "String @+0x010, stride 0x018", 0x018, 1, { 0x010 } };
constexpr DestroySpec kD_Str_20 = { "String @+0x000, stride 0x020", 0x020, 1, { 0x000 } };  // VertexLayout::Element (2026-09-12)
constexpr DestroySpec kD_Str2_38 = { "2 Strings @+0x008,+0x000, stride 0x038", 0x038, 2, { 0x008, 0x000 } };
// WAVE 27 LANE FRONTIER - UI::StringTable's TextEntry {name, text}, the element
// of the array at StringTable+0x20. Read out of 0x180005570's own body:
//   0x1800055a7  MOV RAX,[RBX+0x8]   the FIRST slot released is +0x08
//   0x180005600  MOV RAX,[RBX]       the SECOND is +0x00
//   0x180005655  ADD RBX,0x10        stride
// so the order is DESCENDING, like kD_Str2_38, and the assert below enforces it.
constexpr DestroySpec kD_Str2_10 = { "2 Strings @+0x008,+0x000, stride 0x010", 0x010, 2, { 0x008, 0x000 } };

// ---- Cutover closure batch 2 (2026-09-12): Graphics::Material's parameter
// array and the shader-program compiled-blob array. Both read out of their own
// callee bodies (0x180005880 / 0x18004ff80), not inherited from a sibling row
// -- see the notes above kD_Str_18 for why an offset must never be assumed
// from a stride match alone.
//   0x180005880 (Collection_Array_MaterialParam_DestroyStrings): `*param_2`
//       (offset +0x00) is the released String*, `param_2 += 3` -> stride 0x18.
//       NOT kD_Str_18, which is String at +0x10.
constexpr DestroySpec kD_Str_18at00 = { "String @+0x000, stride 0x018", 0x018, 1, { 0x000 } };
//   0x18004ff80 (Collection_Array_ShaderBlob_DestroyInner): frees the pointer
//       at +0x00, zeroes both qwords of the 16-byte slot -> Body A, stride
//       0x10, offset 0x00. No existing kD_InnerArr_* row has offset 0.
constexpr DestroySpec kD_InnerArr_10at00 = { "inner Array @+0x000, stride 0x010", 0x010, 1, { 0x000 } };

// ---- WAVE 25 LANE INTERNAL42: two more Body-B rows -------------------------
// Both read out of their own helper's disassembly. The helpers walk a POINTER
// rather than indexing (`LEA RBX,[RDX+off]` ... `ADD RBX,stride`), which is the
// same body as 0x180011010's indexed form with a non-zero offset; the loop
// shape differs, the rule does not.
//   0x18002afc0  LEA RBX,[RDX+0x08] ... ADD RBX,0x10   -> stride 0x10, off 0x08
//   0x180039640  LEA RBX,[RDX+0x08] ... ADD RBX,0x10   -> the SAME row
//   0x180039580  LEA RBX,[RDX+0x18] ... ADD RBX,0x20   -> stride 0x20, off 0x18
// kD_Str_10 is stride 0x10 at offset 0x00 and is NOT these: the suffix in these
// names is the STRIDE, so a row at the same stride and a different offset needs
// its own name or the next reader reuses the wrong one.
constexpr DestroySpec kD_Str_10at08 = { "String @+0x008, stride 0x010", 0x010, 1, { 0x008 } };
constexpr DestroySpec kD_Str_20at18 = { "String @+0x018, stride 0x020", 0x020, 1, { 0x018 } };

// ---------------------------------------------------------------------------
// Compile-time checks on the transcription.
//
// This project static_asserts every struct offset; the equivalent here is that
// every (offset, stride) pair a slot is read out of has to FIT. Body A writes
// sixteen bytes at each offset and Body B writes eight, so an offset that
// overruns its own stride is a wild write into the NEXT element -- the failure
// family reference_internalfactory_swapremove_wild_write.md and
// reference_load_wild_write_high_dword_7fc.md are both about, and neither had a
// proximate symptom.
//
// These caught nothing when they were written. That is the point: they are free
// and they run on every build, whereas the probe needs a run and nine of these
// eighteen addresses are teardown-only.
// ---------------------------------------------------------------------------
#define RTCOLL_FITS(SPEC, N, W)                                               \
    static_assert((SPEC).nOffs == (N), #SPEC " nOffs");                       \
    static_assert((SPEC).offs[0] + (W) <= (SPEC).stride, #SPEC " offs[0]")

RTCOLL_FITS(kD_InnerArr_110, 1, 16u);
RTCOLL_FITS(kD_InnerArr_18,  1, 16u);
RTCOLL_FITS(kD_InnerArr_40,  1, 16u);
RTCOLL_FITS(kD_InnerArr_48,  1, 16u);
RTCOLL_FITS(kD_InnerArr3_48, 3, 16u);
static_assert(kD_InnerArr3_48.offs[1] + 16u <= kD_InnerArr3_48.stride, "offs[1]");
static_assert(kD_InnerArr3_48.offs[2] + 16u <= kD_InnerArr3_48.stride, "offs[2]");
RTCOLL_FITS(kD_InnerArr3_1f0, 3, 16u);
static_assert(kD_InnerArr3_1f0.offs[1] + 16u <= kD_InnerArr3_1f0.stride, "offs[1]");
static_assert(kD_InnerArr3_1f0.offs[2] + 16u <= kD_InnerArr3_1f0.stride, "offs[2]");

RTCOLL_FITS(kD_Str_08, 1, 8u);
RTCOLL_FITS(kD_Str_10, 1, 8u);
RTCOLL_FITS(kD_Str_48, 1, 8u);
RTCOLL_FITS(kD_Str_18, 1, 8u);
RTCOLL_FITS(kD_Str2_38, 2, 8u);
RTCOLL_FITS(kD_Str2_10, 2, 8u);
static_assert(kD_Str2_38.offs[1] + 8u <= kD_Str2_38.stride, "offs[1]");

// The three DESCENDING rows, asserted as descending. "Sorted them while tidying
// up" is a silent change to the order the ALLOCATOR sees, and no comparator in
// the probe can see it (the slots do not alias) -- so it has to be checked here
// or not at all.
static_assert(kD_InnerArr3_1f0.offs[0] > kD_InnerArr3_1f0.offs[1] &&
              kD_InnerArr3_1f0.offs[1] > kD_InnerArr3_1f0.offs[2],
              "eh_vector_destructor_iterator destroys in REVERSE order");
static_assert(kD_InnerArr3_48.offs[0] > kD_InnerArr3_48.offs[1] &&
              kD_InnerArr3_48.offs[1] > kD_InnerArr3_48.offs[2],
              "0x18005bce0 walks 0x38, 0x28, 0x08 in that order");
static_assert(kD_Str2_10.offs[0] > kD_Str2_10.offs[1],
              "0x180005570 releases +0x08 BEFORE +0x00 (0x1800055a7 then 0x180005600)");
static_assert(kD_Str2_38.offs[0] > kD_Str2_38.offs[1],
              "0x1800059e0 releases +0x08 BEFORE +0x00");

#undef RTCOLL_FITS

// ============================================================================
// Live instrumentation — a COVERAGE COUNTER, and one real check
// ============================================================================
//
// One RtDiffSite per REGISTERED ADDRESS, not per body, for lane Q's reason:
// five addresses share Body B, so a run that exercises the body proves nothing
// about the other four REGISTRATIONS, and per-address `calls=` is the only
// thing that says which of the eighteen a run actually reached.
//
// What is checked live is deliberately weak and is labelled as such. For Body A
// the post-state is all-zero headers, which restates the rule; for Body B it is
// the arm each slot took, which restates it too. THE INDEPENDENT ORACLE IS THE
// SYNTHETIC-SUBJECT REPLAY in array_destroy_probe.cpp, which compares against
// the SHIPPED body. Do not read a green [RtDiff] summary here as verification —
// reference_uncalled_port_is_not_verified.md is about exactly this file's
// shape, where nine of the eighteen addresses are teardown-only.
//
// *** A Site MUST HAVE STATIC STORAGE DURATION *** — array.cpp §"Live
// instrumentation" records what a stack Site cost the integrator (144 dangling
// registrations reported as "a stray write hit the harness's own statics").
// Every Site below is file-scope.

// The probe self-schedules for the same reason lane Q's does: RtCollection*Init
// runs immediately after ResolveInit()'s LoadLibrary, when the DLL's
// MemoryAllocator is CONSTRUCTED BUT NOT STARTED. The first call into any entry
// point below is a moment we know is safe, because the caller is itself in the
// middle of destroying an Array.
bool g_dprobePending = false;
static bool g_dprobeRunning = false;

static inline void DProbeGate() {
    if (!g_dprobePending || g_dprobeRunning) return;
    g_dprobeRunning = true;
    RtCollectionDestroyProbeRun();
    g_dprobePending = false;
    g_dprobeRunning = false;
}

// Reports how many slots the call actually touched, so `calls=` can be read as
// "did work" and not merely "was entered"
// (reference_probe_needs_an_effectiveness_control.md).
static void ReportDestroy(Site* site, const DestroySpec* s, int count, int freed) {
    if (!site || !RtDiffEnabled()) return;
    RtDiffRegister(&site->d);
    char ours[128];
    std::snprintf(ours, sizeof(ours), "%s n=%d slots=%d freed=%d",
                  s->name, count, count * s->nOffs, freed);
    RtDiffReport(&site->d, true, ours, ours);
}

// Counting wrappers. These do the work AND the counting in one pass rather than
// re-walking the buffer afterwards, because after Body A the slots are zeroed
// and after Body B they are the answer — a second pass could not tell what the
// first one did.
static int FreeInnerArraysCounted(uint8_t* base, int count, const DestroySpec* s) {
    int freed = 0;
    if (count > 0) {
        const int64_t n = static_cast<int64_t>(count);
        for (int64_t i = 0; i < n; ++i)
            for (int k = 0; k < s->nOffs; ++k)
                if (*reinterpret_cast<void**>(base + i * s->stride + s->offs[k])) ++freed;
    }
    FreeInnerArraysCore(base, count, s);
    return freed;
}

static int ReleaseStringsCounted(uint8_t* base, int count, const DestroySpec* s) {
    int nonNull = 0;
    if (count > 0) {
        const int64_t n = static_cast<int64_t>(count);
        for (int64_t i = 0; i < n; ++i)
            for (int k = 0; k < s->nOffs; ++k)
                if (*reinterpret_cast<void**>(base + i * s->stride + s->offs[k])) ++nonNull;
    }
    ReleaseStringsCore(base, count, s);
    return nonNull;
}

}  // namespace rtcoll

// ============================================================================
// The registered entry points
// ============================================================================
namespace rtcoll {

// ---- the thirteen helper addresses ----------------------------------------
// Signature is retail's: (ignoredThis, base, count). `ignoredThis` is spilled
// to the shadow space and never read by any of them; it stays in the signature
// because these are registered AT retail's addresses and must match retail's
// arity, not a cleaned-up one.

#define A_ENTRY(ADDR, SPEC)                                                   \
    Site g_da_##ADDR = { { "Collection::Array elem-destroy(inner Array) @0x" #ADDR, \
                           0, 0, false, false }, 0x##ADDR };                  \
    void RtColl_DestroyInner_##ADDR(void* ignoredThis, void* base, int count) { \
        (void)ignoredThis;                                                    \
        DProbeGate();                                                         \
        const int freed = FreeInnerArraysCounted(                             \
            static_cast<uint8_t*>(base), count, &SPEC);                       \
        ReportDestroy(&g_da_##ADDR, &SPEC, count, freed);                     \
    }

// ONLY THE HELPERS OUR TREE ACTUALLY RESOLVES GET AN ENTRY POINT.
//
// Of the eleven destroy-helper addresses, exactly three are reached through
// `ResolveAddr(Module::Runtime, …)` anywhere in this tree — the three lane Q's
// non-trivial specs call. The other eight are reached only by RETAIL's own
// bodies, which we are replacing, so from our side they have no callers at all.
//
// They are PORTED all the same (the J2 entry points below inline exactly the
// body and spec each one contains) and they are deliberately NOT REGISTERED,
// because tools/runtime_census.py reports a registration nothing calls under
// "Ports with no caller in our tree — check each is not a vestigial resolve".
// Eight permanent entries there would dilute the one signal this wave has for
// finding a DEAD port (reference_uncalled_port_is_not_verified.md). Registering
// them buys nothing at cutover either: with the DLL gone, only OUR callers
// exist, and our callers go through the J2 entry points.
//
// The eight, with the spec that already covers each, so a future caller is one
// A_ENTRY/B_ENTRY line away and no re-derivation:
//     0x18001ce90 kD_InnerArr_110   0x1800419b0 kD_InnerArr_18
//     0x180074100 kD_InnerArr_48    0x18005bce0 kD_InnerArr3_48
//     0x180011010 kD_Str_08         0x180014a40 kD_Str_48
//     0x180034440 kD_Str_18         0x18005c550 DestroyNestedPair
//
// *** WAVE 9 LANE UIW, 2026-09-08 — TWO OF THE EIGHT NOW HAVE A CALLER, and
// the note above says exactly what to do about it: "Register it the day a
// caller appears." Taken up literally, one line each, no re-derivation. ***
//
//   0x1800419b0 (kD_InnerArr_18) — the shrink/realloc destroy arm of
//       `Array<GraphSeries>::SetSize` (0x1800417d0), ported this commit in
//       array.cpp as `kSpec_GraphSeries` to close
//       `?AddGraph@GraphElement@UI@@`.
//   0x180011010 (kD_Str_08) — the same arm of `Array<System::String>::SetSize`
//       (0x180034860), ported this commit as `kSpec_StringRef` to close
//       `?AddItem@ComboElement@UI@@`. It will get a SECOND caller the day
//       0x180029a80 (the two-argument sibling, `TextElement::
//       SetParameterCount`'s) lands.
//
// This is the good outcome of a mistake worth recording. Lane UIW's first draft
// wrote its OWN body for both of these and registered 0x180010ab0 as well —
// which this file already registers as a J2 entry — producing a shadowed
// address and two live copies of one function. It surfaced only because a diff
// run printed a site name (`…~Array(+elem) @0x180010ab0`) that lane had not
// written. **array.cpp and array_destroy.cpp are two files and one namespace;
// grep the address across both before porting a Collection helper.**
A_ENTRY(180052110, kD_InnerArr_40)    // Audio::EffectSlot array (NOT Audio-gated — see banner)
A_ENTRY(1800419b0, kD_InnerArr_18)    // UI: GraphElement::_graphs, the nested point Array
// Cutover closure batch 2: Video::ShaderProgram's compiled-blob array element
// destroy (frees the per-target bytecode buffer, offset 0, stride 0x10). Has
// a caller from the first commit (the J2_ENTRY for 0x18004fbd0 below), so it
// is registered rather than left as one of "the eight".
A_ENTRY(18004ff80, kD_InnerArr_10at00) // Video: ShaderProgram's compiled-blob Array<Array<byte>>

#undef A_ENTRY

#define B_ENTRY(ADDR, SPEC)                                                   \
    Site g_db_##ADDR = { { "Collection::Array elem-destroy(String) @0x" #ADDR, \
                           0, 0, false, false }, 0x##ADDR };                  \
    void RtColl_DestroyStrings_##ADDR(void* ignoredThis, void* base, int count) { \
        (void)ignoredThis;                                                    \
        DProbeGate();                                                         \
        const int n = ReleaseStringsCounted(                                  \
            static_cast<uint8_t*>(base), count, &SPEC);                       \
        ReportDestroy(&g_db_##ADDR, &SPEC, count, n);                         \
    }

B_ENTRY(180005930, kD_Str_10)         // {String*, ExternalPtr} — lane Q's kSpec_StringExt
B_ENTRY(1800059e0, kD_Str2_38)        // SpriteSheet's 0x38 entry — lane Q's kSpec_Sprite38
B_ENTRY(180011010, kD_Str_08)         // UI: ComboElement::_strings — lane UIW's kSpec_StringRef
B_ENTRY(180026b50, kD_Str_10)         // IO::PackageFile::Entry — lane Q's kSpec_PkgEntry
B_ENTRY(18004fe20, kD_Str_20)         // Video::VertexLayout::Element — array.cpp kSpec_VertexElement (2026-09-12)
// WAVE 26 LANE INT33 — the fifth, and it is one of "the eight ported but
// deliberately unregistered" the banner above lists. The stated condition for
// registering one of the eight is "when a caller exists", and one now does:
// array.cpp's kSpec_ExtStr10at08 (0x180075d20's SetSize, ported in the same
// commit) reaches it by address, exactly as retail's 0x180075d20 reaches it by
// rel32. Offset re-read here rather than inherited: `LEA RBX,[RDX+0x8]` at
// 0x18003966c, `ADD RBX,0x10` -- String at +0x08, stride 0x10 == kD_Str_10at08.
B_ENTRY(180039640, kD_Str_10at08)     // UI: DialogNamedTemplate {ptr, String*}
// ...and the sixth, on the same terms. `LEA RBX,[RDX+0x18]` at 0x1800395ac and
// `ADD RBX,0x20` at 0x18003961e -- String at +0x18, stride 0x20 ==
// kD_Str_20at18. Its caller is array.cpp's kSpec_CfgEntry20 (0x180075b20).
// NOTE these two lines are the pair Wave 25 nearly got wrong: 0x180039640 and
// 0x180039580 are called from bodies that share an opcode hash, and the hash
// erases the CALL operand that tells them apart. Each offset above is read from
// the HELPER's own disassembly, not from its caller's hash.
B_ENTRY(180039580, kD_Str_20at18)     // UI: DialogControllerConfigEntry

// ---- WAVE 27 LANE FRONTIER: the SIXTH Body-B address, 0x180006ac0 ----------
//
// `Object::ExternalArrayElement`'s range destroy -- the element-destroy arm of
// `Collection::Array<ExternalArrayElement>::SetSize` (0x18002c6a0), which is
// the wall cutover attempt 29 died on (EXEC-of-0 at ArraySetSize+0x4d).
//
// *** IT IS BODY B AND THAT IS MEASURED, NOT RESEMBLED. *** 0x180006ac0 was
// disassembled in full and set beside 0x180005930, the row directly above.
// They are identical instruction for instruction and in every resolved
// absolute operand (0x1801fc718, 0x18014b390, 0x18014b388, 0x1800c6c00) --
// the two differences are EXACTLY the pair array.cpp's banner already
// tabulates between 0x180026b50 and 0x180005930 and that no caller can see:
//     0x180006af2  NOP dword ptr [RAX + RAX*0x1]   alignment padding
//     0x180006b05  MOV qword ptr [RSP+0x50],RBX    a DUPLICATED store into
//                                                  this body's own SEH scope
//                                                  slot (0x180006b00 makes it
//                                                  first, with the same value)
// The spec is read out of THIS body: `ADD RBX,0x10` at 0x180006b5f is the
// stride, and the slot is `MOV RAX,qword ptr [RBX]` at 0x180006b0a -- offset
// 0x00. That is kD_Str_10, no new row. Third instance of one body; the first
// two are the two lines above.
B_ENTRY(180006ac0, kD_Str_10)         // Object: ExternalArrayElement {Filename, 2x int32}

// ---- WAVE 27 LANE FRONTIER: 0x180005570, the FIRST TWO-SLOT Body-B at 0x10 --
//
// `UI::StringTable`'s TextEntry range destroy. Reached from TWO places, and
// both are ported in this same commit:
//   * 0x180004a90, `~Array<TextEntry>` (the J2 entry below), which is what
//     `??1StringTable@UI@@` calls on `this+0x20`;
//   * 0x180078c90, `Array<TextEntry>::SetSize`, on BOTH of its destroy arms -
//     array.cpp's kSpec_StringPair10.
//
// It is Body B with a TWO-OFFSET spec, which no previous Body-B row at stride
// 0x10 has been: kD_Str_10 (0x180005930 / 0x180026b50 / 0x180006ac0) releases
// ONE slot at +0x00 and this one releases TWO. Sharing kD_Str_10 by stride
// would have left every TextEntry's `text` String leaked, silently, with a
// clean build - the fifth time this family has offered a wrong answer by
// resemblance. The spec is read out of THIS body (see kD_Str2_10 above).
B_ENTRY(180005570, kD_Str2_10)        // UI: StringTable::TextEntry {name, text}

// ---------------------------------------------------------------------------
// 0x180026b50 — WAVE 9 LANE DESTROY, and why it shares kD_Str_10 WITHOUT this
// being the fifth near-miss-by-resemblance this family has produced
// ---------------------------------------------------------------------------
// This address is the CUTOVER WALL of attempt 17: `ReadPackfile` -> the
// `Array<IO::PackageFile::Entry>` SetSize at 0x1800298f0 -> `SetSizeCore` ->
// `DestroyPkgEntryRange`, which forwarded here BY ADDRESS. Unexported and
// unregistered, so with the DLL absent the call executed address 0.
//
// array.cpp's row for kSpec_PkgEntry said this was "a different function from
// DestroyStringExt's 0x180005930", and that sentence was doing two jobs at
// once. It is true of the ADDRESS and it is false of the BODY, and only the
// first was ever checked. Both listings, read out of Runtime-x64.dll for this
// commit rather than inferred from each other:
//
//   0x180026b50                              0x180005930
//   TEST R8D,R8D / JLE ret                   TEST R8D,R8D / JLE ret
//   MOV [RSP+8],RCX      ; spilled, never    MOV [RSP+8],RCX      ; ... read
//   PUSH RSI/RDI/R14 ; SUB RSP,0x30          PUSH RSI/RDI/R14 ; SUB RSP,0x30
//   MOV [RSP+0x20],-2   ; SEH state          MOV [RSP+0x20],-2
//   MOV RBX,RDX ; MOVSXD RSI,R8D             MOV RBX,RDX ; MOVSXD RSI,R8D
//   XOR R14D,R14D                            XOR R14D,R14D
//   NOP dword [RAX+RAX]  ; 14-byte align     (no padding)
//   MOV [RSP+0x50],RBX   ; *** TWICE ***     MOV [RSP+0x50],RBX   ; once
//   MOV [RSP+0x50],RBX
//   MOV RAX,[RBX] / TEST / JZ next           MOV RAX,[RBX] / TEST / JZ next
//   DEC [RAX-4] / CMP [RAX-4],0 / JNZ        (identical)
//   ADD qword [RBX],-8 / MOV RBP,[RBX] / JZ  (identical)
//   Enter / Free(_singleton,RBP) / Leave     (identical)
//   MOV [RBX],R14        ; the shared arm    (identical)
//   ADD RBX,0x10 / DEC RSI / JNZ loop        ADD RBX,0x10 / DEC RSI / JNZ loop
//
// The ONLY two differences are an alignment `NOP` and a DUPLICATED store of RBX
// into the SEH scope slot at [RSP+0x50] — MSVC emitting the frame-variable
// update once per inlined scope. Neither is observable: [RSP+0x50] is this
// function's own frame, written and never read. Every store to the SUBJECT,
// every branch, every immediate, the stride (`ADD RBX,0x10`) and the single
// offset (bare `[RBX]`, i.e. +0x00) agree.
//
// So this IS Body B with kD_Str_10, and the reason that is allowed to be said
// here is that the sentence is a conclusion from the listing above and not from
// the fact that the two are alike. The four near-misses this family has already
// produced (an alignment immediate, a missing SHL, a TRAILING CALL to
// Filename::AdjustDelimiters, and a 0x32/0x33 that meant 51 vs 52) all hid in
// exactly the columns tabulated above, which is why they are tabulated rather
// than summarised. The probe compares against the SHIPPED 0x180026b50 — not
// against 0x180005930 — so the claim is also tested against the right body.
//
// The trailing-call trap in particular does NOT recur here: this listing's only
// CALLs are the allocator's Enter / Free / Leave, the same three 0x180005930
// makes, and there is nothing after the loop but the epilogue.
//
// NOT a new spec row, and that is the load-bearing negative: `kD_Str_10` is
// (stride 0x10, offs {0x00}) and this body computes those two numbers itself.
// The ELEMENT is IO::PackageFile::Entry — System::Filename at +0x00, int32
// offset at +0x08, int32 size at +0x0c — so the two dwords in the tail are
// exactly the eight bytes Body B does not touch, and the `if (isSlot)` skip in
// the probe's byte-for-byte check (step 3) covers them.

// Cutover closure batch 2: Graphics::Material's parameter array (String* name
// at +0x00, stride 0x18, ~MaterialInstance's call). Has a caller from the
// first commit (0x180004c80's J2_ENTRY below), so registered immediately.
B_ENTRY(180005880, kD_Str_18at00)      // Graphics: Material parameter array

#undef B_ENTRY

// ---- J2: the six ~Array bodies with an element-destroy prologue ------------
//
// `ArrayDtorImpl` is lane Q's J1 body, called rather than restated: J2 IS
// helper-then-J1, and a second copy of J1 here would be a place for the two to
// drift apart.
//
// The helper is reached through the same DestroySpec this file registers, NOT
// through ResolveAddr — these six know statically which helper retail calls, so
// going out to the registry and back would be a slower way to reach the same
// function and would hide the edge from tools/runtime_census.py.

#define J2_ENTRY(ADDR, CORE, SPEC)                                            \
    Site g_j2_##ADDR = { { "Collection::Array::~Array(+elem) @0x" #ADDR,       \
                           0, 0, false, false }, 0x##ADDR };                  \
    void* RtColl_ArrayDtorElem_##ADDR(void* self) {                           \
        DProbeGate();                                                         \
        ArrayHdr* a = static_cast<ArrayHdr*>(self);                           \
        /* retail: R8D=[RCX+8], RDX=[RCX], RCX unchanged, then CALL helper */  \
        const int freed = CORE(a->_data, a->_size, &SPEC);                    \
        void* r = ArrayDtorImpl(a);                                           \
        if (RtDiffEnabled()) {                                                \
            RtDiffRegister(&g_j2_##ADDR.d);                                   \
            const bool ok = (a->_data == nullptr && a->_size == 0 &&          \
                             a->_capacity == 0);                              \
            char o[128];                                                      \
            std::snprintf(o, sizeof(o), "elems=%d data=%p size=%d cap=%d",    \
                          freed, static_cast<void*>(a->_data), a->_size,      \
                          a->_capacity);                                      \
            RtDiffReport(&g_j2_##ADDR.d, ok, o,                               \
                         "data=0000000000000000 size=0 cap=0");               \
        }                                                                     \
        return r;                                                             \
    }

// The helper each entry point calls is the one its own disassembly CALLs, and
// the pairing is not guessable from the address: 0x180033880 sits between two
// Body-A neighbours and calls a Body-B helper. Each line names the address it
// was read from.
J2_ENTRY(180010ab0, ReleaseStringsCounted,  kD_Str_08)       // CALL 0x180011010  Body B
J2_ENTRY(18001c7e0, FreeInnerArraysCounted, kD_InnerArr_110) // CALL 0x18001ce90  Body A
J2_ENTRY(180033880, ReleaseStringsCounted,  kD_Str_18)       // CALL 0x180034440  Body B
J2_ENTRY(1800416d0, FreeInnerArraysCounted, kD_InnerArr_18)  // CALL 0x1800419b0  Body A
J2_ENTRY(180073c90, FreeInnerArraysCounted, kD_InnerArr_48)  // CALL 0x180074100  Body A
J2_ENTRY(18004fa30, ReleaseStringsCounted,  kD_Str_20)       // CALL 0x18004fe20  Body B  ~VertexLayout (2026-09-12)

// ---- WAVE 25 LANE INTERNAL42: three more J2 addresses ----------------------
//
// *** THE TRAP THESE THREE CARRY, AND IT NEARLY COST TWO WRONG SPECS. ***
// Wave 23 established by address-normalised opcode hash that
//     0x18002ac40 == 0x180039280 == 0x180039360
// are ONE body, 127 bytes, hash 089ee8. That is true, and it does NOT mean they
// take the same DestroySpec. An address-normalised hash erases the rel32 field
// of the `CALL helper` at the top — which is the ONLY thing that distinguishes
// a J2 entry from its siblings. Read by filtering each body's own CALLs:
//     0x18002ac40 -> CALL 0x18002afc0   stride 0x10, String at +0x08
//     0x180039360 -> CALL 0x180039640   stride 0x10, String at +0x08  (same row)
//     0x180039280 -> CALL 0x180039580   stride 0x20, String at +0x18  (DIFFERENT)
// So "byte-identical" here means "identical except for the one operand that
// decides the answer". Taking the hash as authority would have run a
// stride-0x20 array at stride 0x10 and walked off the end of the buffer.
J2_ENTRY(18002ac40, ReleaseStringsCounted,  kD_Str_10at08)   // CALL 0x18002afc0  Body B
J2_ENTRY(180039360, ReleaseStringsCounted,  kD_Str_10at08)   // CALL 0x180039640  Body B
J2_ENTRY(180039280, ReleaseStringsCounted,  kD_Str_20at18)   // CALL 0x180039580  Body B

// ---- WAVE 26 LANE INT33: the tenth J2 address -----------------------------
//
// 0x1800269a0 — `~Array<IO::PackageFile::Entry>`, `~PackageFile`'s member at
// +0xB0 (runtime/src/io/package_file_dtor.cpp:31 named it and recorded it as
// "NOT PORTED <- the only one" of that dtor's seven callees).
//
// MEASURED, not inherited, with `tools/int33_normdiff.py` (9/9 controls):
//
//   0x1800269a0 vs 0x18002ac40   extent 127/127   raw byte diff 13
//     the ONLY normalised difference is the one operand that decides the
//     answer:  +0x23 CALL 0x180026b50   vs   +0x23 CALL 0x18002afc0
//   0x1800269a0 --calls
//     +0x23 CALL 0x180026b50     the element-destroy helper
//     +0x46 CALL 0x18014b390  }
//     +0x57 CALL 0x1800c6c00  }  the allocator Enter / Free / Leave triple,
//     +0x60 CALL 0x18014b388  }  byte-for-byte the triple 0x180010ab0 makes
//
// The helper is ALREADY PORTED AND REGISTERED in this file as
// `RtColl_DestroyStrings_180026b50` under `kD_Str_10`, and the B_ENTRY line for
// it (above) already names its element as `IO::PackageFile::Entry`. So this is
// an ENTRY POINT for machinery that exists, not a new body, and it needs NO new
// DestroySpec row — which is asserted from 0x1800269a0's own CALL operand and
// not from a hash, because a hash erases exactly that operand (see the three
// Wave 25 rows above, where two siblings sharing hash 089ee8 take two specs).
J2_ENTRY(1800269a0, ReleaseStringsCounted,  kD_Str_10)       // CALL 0x180026b50  Body B

// ---- WAVE 27 LANE FRONTIER: the eleventh J2 address ------------------------
//
// 0x180006b80 -- `~Array<Object::ExternalArrayElement>`. Read out of its own
// disassembly, not inferred from the neighbour:
//     180006b9c  MOV R8D,dword ptr [RCX+0x8]     count  = _size
//     180006ba0  MOV RDX,qword ptr [RCX]         base   = _data
//     180006ba3  CALL 0x180006ac0                <-- the helper, by rel32
//   then the allocator Enter / Free / Leave triple through 0x1801fc718+0x640,
//   0x18014b390, 0x1800c6c00, 0x18014b388, and finally `XOR EAX,EAX` with the
//   two stores at [RDI] and [RDI+0x8] -- which is ArrayDtorImpl exactly.
// So: entry point for machinery that now exists, no new spec.
//
// It is REGISTERED rather than left as one of "the eight", because at cutover
// it is how `Object::External`'s dependency array is torn down and retail's
// rel32 to 0x180006ac0 will not exist. Registering both halves is the lesson
// the 0x1800269a0 line above records.
J2_ENTRY(180006b80, ReleaseStringsCounted,  kD_Str_10)       // CALL 0x180006ac0  Body B

// ---- WAVE 27 LANE FRONTIER: the twelfth J2 address -------------------------
//
// 0x180004a90 - `~Array<UI::StringTable::TextEntry>`, the member destroy in
// `??1StringTable@UI@@` (0x18011d914 `ADD RCX,0x20 ; CALL 0x180004a90`). Read
// out of its own disassembly, and its CALL operand is the field that decides
// the spec (the Wave 25 lesson three lines of this file already carry):
//     180004aac  MOV R8D,[RCX+0x8]      count = _size
//     180004ab0  MOV RDX,[RCX]          base  = _data
//     180004ab3  CALL 0x180005570       <-- kD_Str2_10, NOT kD_Str_10
//   then the allocator Enter / Free / Leave triple and the two zero stores
//   that are ArrayDtorImpl.
J2_ENTRY(180004a90, ReleaseStringsCounted,  kD_Str2_10)      // CALL 0x180005570  Body B

// ---- Cutover closure batch 2 (2026-09-12): the thirteenth and fourteenth --
//
// 0x180004c80 -- `~Array<{String*,MaterialParam}>`, ~MaterialInstance's
// parameter array. Read out of its own body: `MOV R8D,[RCX+8]` / `MOV
// RDX,[RCX]` / `CALL 0x180005880` then the allocator triple and the two zero
// stores -- the same J2 shape, a new (String, stride 0x18, offset 0x00) row.
J2_ENTRY(180004c80, ReleaseStringsCounted,  kD_Str_18at00)   // CALL 0x180005880  Body B
// 0x18004fbd0 -- `~Array<Array<byte>>`, the shader-program compiled-blob
// array (called from ~ShaderProgram / ~VertexProgram / ~PixelProgram). CALLs
// 0x18004ff80, Body A with offset 0x00, stride 0x10.
J2_ENTRY(18004fbd0, FreeInnerArraysCounted, kD_InnerArr_10at00) // CALL 0x18004ff80  Body A
// 0x180051fe0 -- another `~Array` calling 0x180052110 (already ported/
// registered above as A_ENTRY's Audio::EffectSlot row, kD_InnerArr_40). Read
// from its own body: `CALL 0x180052110` then the same allocator triple.
J2_ENTRY(180051fe0, FreeInnerArraysCounted, kD_InnerArr_40)  // CALL 0x180052110  Body A

#undef J2_ENTRY

// ---- 0x18005bf60 / 0x18005c550: the one that is NEITHER body ---------------
//
// Lane D filed 0x18005bf60 as an ordinary J2 with "one more helper row". Its
// helper 0x18005c550 is not Body A and not Body B: each of ITS elements holds
// two nested Arrays whose OWN elements need destroying first, so it is a
// RECURSIVE destroy that calls one of each body before freeing.
//
// Read at 0x18005c550 (RBX = base + 0x28, stride 0x30, so the displacements
// below are RBX-relative in the listing and element-relative here):
//
//   for each element e:
//       0x18005bce0(ignored, *(void**)(e+0x20), *(int32*)(e+0x28));  // Body A x3
//       free *(void**)(e+0x20);  e+0x20 = 0;  e+0x28 = 0;
//       0x180014a40(ignored, *(void**)(e+0x10), *(int32*)(e+0x18));  // Body B
//       free *(void**)(e+0x10);  e+0x10 = 0;  e+0x18 = 0;
//
// Element layout, therefore: {…, Array<String-bearing> at +0x10,
// Array<Array-bearing> at +0x20}, stride 0x30. Both sub-helpers are registered
// addresses of this file, so this creates no new row either — but it is a THIRD
// body and had to be written out rather than parameterized, because the inner
// element count is read from the nested header and not passed in.
//
// ORDER IS LOAD-BEARING HERE in a way it is not elsewhere in this file: the
// +0x20 array is destroyed and freed BEFORE the +0x10 one is even read, and the
// two frees reach the allocator in that order.
static int DestroyNestedPair(uint8_t* base, int count) {
    int freed = 0;
    if (count <= 0) return 0;
    const int64_t n = static_cast<int64_t>(count);
    for (int64_t i = 0; i < n; ++i) {
        uint8_t* e = base + i * 0x30;

        uint8_t* inner = *reinterpret_cast<uint8_t**>(e + 0x20);
        FreeInnerArraysCore(inner, *reinterpret_cast<int32_t*>(e + 0x28), &kD_InnerArr3_48);
        if (inner) { MemFree(inner); ++freed; }
        *reinterpret_cast<uint64_t*>(e + 0x20) = 0;
        *reinterpret_cast<uint64_t*>(e + 0x28) = 0;

        uint8_t* strs = *reinterpret_cast<uint8_t**>(e + 0x10);
        ReleaseStringsCore(strs, *reinterpret_cast<int32_t*>(e + 0x18), &kD_Str_48);
        if (strs) { MemFree(strs); ++freed; }
        *reinterpret_cast<uint64_t*>(e + 0x10) = 0;
        *reinterpret_cast<uint64_t*>(e + 0x18) = 0;
    }
    return freed;
}

Site g_j2_18005bf60 = { { "Collection::Array::~Array(+nested elem) @0x18005bf60",
                          0, 0, false, false }, 0x18005bf60 };
void* RtColl_ArrayDtorElem_18005bf60(void* self) {
    DProbeGate();
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    const int freed = DestroyNestedPair(a->_data, a->_size);   // CALL 0x18005c550
    void* r = ArrayDtorImpl(a);
    if (RtDiffEnabled()) {
        RtDiffRegister(&g_j2_18005bf60.d);
        const bool ok = (a->_data == nullptr && a->_size == 0 && a->_capacity == 0);
        char o[128];
        std::snprintf(o, sizeof(o), "nested=%d data=%p size=%d cap=%d", freed,
                      static_cast<void*>(a->_data), a->_size, a->_capacity);
        RtDiffReport(&g_j2_18005bf60.d, ok, o, "data=0000000000000000 size=0 cap=0");
    }
    return r;
}

// ---- J2b: 0x18001c8c0, the eh_vector form ---------------------------------
// Body A with offs {0x1e0,0x1d0,0x1c0}, stride 0x1f0 — see the banner for the
// two hops (0x18001b960 is a 5-byte JMP thunk to a twelfth copy of J1) that
// make it Body A and not a CRT dependency.
Site g_j2b_18001c8c0 = { { "Collection::Array::~Array(eh_vector) @0x18001c8c0",
                           0, 0, false, false }, 0x18001c8c0 };
void* RtColl_ArrayDtorEhVec_18001c8c0(void* self) {
    DProbeGate();
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    const int freed = FreeInnerArraysCounted(a->_data, a->_size, &kD_InnerArr3_1f0);
    void* r = ArrayDtorImpl(a);
    if (RtDiffEnabled()) {
        RtDiffRegister(&g_j2b_18001c8c0.d);
        const bool ok = (a->_data == nullptr && a->_size == 0 && a->_capacity == 0);
        char o[128];
        std::snprintf(o, sizeof(o), "elems=%d data=%p size=%d cap=%d", freed,
                      static_cast<void*>(a->_data), a->_size, a->_capacity);
        RtDiffReport(&g_j2b_18001c8c0.d, ok, o, "data=0000000000000000 size=0 cap=0");
    }
    return r;
}

// ============================================================================
// 0x180004220 — the SpriteSheet element's copy-assign
// ============================================================================
//
// Lane Q's fifth new row, and the one of the five that is a self-contained leaf:
// 37 instructions, ZERO callees, no allocation. Retired here because it costs
// nothing and it CORROBORATES the destroy side — it assigns exactly the two
// System::String slots at +0x00 and +0x08 that 0x1800059e0 releases, in the
// same two places. Two independently-read bodies agreeing on a layout is worth
// more than either reading alone.
//
//     [dst+0x00] = 0
//     s = [src+0x00];  if (s) { [dst+0x00] = s; ++*(int32*)(s-4); }   // refcount
//     [dst+0x08] = 0
//     s = [src+0x08];  if (s) { [dst+0x08] = s; ++*(int32*)(s-4); }   // refcount
//     nine DWORD copies: +0x10 +0x14 +0x18 +0x1c +0x20 +0x24 +0x28 +0x2c +0x30
//     return dst                                        ; MOV RAX,RCX
//
// The pre-zero before each conditional store is retail's, not tidiness: a
// smart-pointer `operator=` inlined, and it is the visible difference between
// "assigned null" and "left whatever was there". Reproduced because
// BANISHED_REDZONE=watch and a hardware write watchpoint can tell them apart.
//
// *** THE ELEMENT IS 0x38 BYTES AND THIS WRITES 0x00 THROUGH 0x33. BYTES
// +0x34 .. +0x37 ARE NEVER WRITTEN BY THE ASSIGN. *** Lane Q's kSpec_Sprite38
// carries `unwrittenLo = unwrittenHi = 0`, i.e. "no such window", and its probe
// seeds all-zero elements so the gap never showed. Any oracle that memcmps a
// freshly assigned SpriteSheet element will fail on four bytes of allocator
// fill and read like a catastrophic port defect — which is
// reference_oracle_must_not_compare_unwritten_bytes.md exactly, four times over
// in Wave 6 and three more in lane Q.
//
// NOT fixed in lane Q's spec by this lane, deliberately: that field is
// documented there as the window retail's DEFAULT (init) arm leaves alone, and
// this window is on the ASSIGN arm. Widening the field's meaning from another
// lane, with no ability to run lane Q's probe, would be the change that looks
// harmless and silently stops comparing bytes that should be compared. The
// finding is filed in docs/runtime_port/WAVE9_LANE_D2.md as lane Q's to make.
Site g_da_180004220 = { { "SpriteSheet elem operator= @0x180004220", 0, 0, false, false },
                        0x180004220 };
void* RtColl_SpriteEntryAssign_180004220(void* dst, const void* src) {
    DProbeGate();
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    for (uint32_t off = 0x00; off <= 0x08; off += 8) {
        *reinterpret_cast<void**>(d + off) = nullptr;
        void* str = *reinterpret_cast<void* const*>(s + off);
        if (str) {
            *reinterpret_cast<void**>(d + off) = str;
            ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(str) - 4);
        }
    }
    for (uint32_t off = 0x10; off <= 0x30; off += 4)
        *reinterpret_cast<uint32_t*>(d + off) = *reinterpret_cast<const uint32_t*>(s + off);
    /* +0x34 .. +0x37 deliberately untouched — retail does not write them */

    if (RtDiffEnabled()) {
        RtDiffRegister(&g_da_180004220.d);
        // Independent: bytes 0x00..0x33 of dst must now equal src. This compares
        // against data we did not write, unlike the header-algebra checks
        // elsewhere in this lane, which restate the rule they are checking.
        // 0x34 and not 0x38 — see the note above about the four unwritten bytes.
        const bool ok = std::memcmp(d, s, 0x34) == 0;
        char o[96];
        std::snprintf(o, sizeof(o), "str0=%p str1=%p",
                      *reinterpret_cast<void**>(d), *reinterpret_cast<void**>(d + 8));
        RtDiffReport(&g_da_180004220.d, ok, o, "dst[0x00..0x33] == src[0x00..0x33]");
    }
    return dst;
}

// ============================================================================
// J4 (the 0x180003970 half) — Collection::Array<T*>::operator=
// ============================================================================
//
//   Array<T*>& operator=(this, const Array<T*>& src)   stride 8, align 8
//
//     if (this == &src) return this;          ; CMP RCX,RDX / JZ
//     ~Array(this);                           ; CALL 0x180004890 — a J1 body
//     ECX = src._size;  EDX = this->_size;    ; *** read AFTER the dtor ***
//     this->_size = ECX;
//     if (ECX > this->_capacity) {
//         this->_capacity = ECX;
//         if (ECX == EDX+1 && EDX != 0) this->_capacity = ECX*3;
//         this->_data = MemAlloc((int64)(int32)_capacity * 8, 8);   ; MOVSXD, SHL 3
//     }
//     for (i=0, off=0; i < this->_size; ++i, off += 8) {
//         p = this->_data + off;
//         if (p) *(void**)p = *(void**)(src._data + off);   ; tests the COMPUTED addr
//     }
//     return this;                            ; MOV RAX,RBX
//
// TWO THINGS THAT LOOK LIKE TRANSCRIPTION ERRORS AND ARE NOT.
//
// 1. `oldSize` is read at 0x18000398d — AFTER the destructor at 0x180003985 has
//    zeroed `_size`. So `oldSize` is ALWAYS 0, which makes `oldSize != 0` always
//    false and the `* 3` treble DEAD CODE in this instantiation. It is written
//    out anyway, in retail's order, because the port's job is the shipped
//    behaviour and not the behaviour MSVC meant; folding it away would silently
//    diverge if the read order were ever misread.
//
// 2. `_capacity` is likewise 0 after the dtor, so `newSize > _capacity` is taken
//    for every non-empty source and this always reallocates. That is why there
//    is no "copy from `old`" limb here as there is in SetSize: `old` is gone.
//
// The element copy is a plain 8-byte move with NO refcount and NO pre-zero —
// this instantiation's T is a raw pointer. Lane Q's `kSpec_StringExt` shows what
// the refcounting variant looks like; do not merge the two.
//
// The other J4 address, 0x180010110 (`ModArrayCopy`), is NOT ported here: it
// calls 0x1800101e0 (`ModEntry_CopyCtor`), which is lane D's J9 IO::Mod cluster
// and reaches ExpandRelativePath / FindOrLoad / BinaryFileStream. Porting it
// would create a J9 row, which is the trade J9 exists to avoid taking piecemeal.
Site g_j4_180003970 = { { "Collection::Array::operator= @0x180003970", 0, 0, false, false },
                        0x180003970 };
void* RtColl_ArrayAssign_180003970(void* self, const void* src) {
    DProbeGate();
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    const ArrayHdr* s = static_cast<const ArrayHdr*>(src);
    if (a == s) return a;

    ArrayDtorImpl(a);                      // CALL 0x180004890 — lane Q's J1 body

    const int32_t newSize = s->_size;
    const int32_t oldSize = a->_size;      // always 0; see note 1 above
    a->_size = newSize;

    bool realloced = false;
    if (newSize > a->_capacity) {          // signed 32-bit compare
        a->_capacity = newSize;
        if (newSize == static_cast<int32_t>(oldSize + 1) && oldSize != 0)
            a->_capacity = static_cast<int32_t>(newSize * 3);   // dead; kept faithful
        a->_data = static_cast<uint8_t*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity)) * 8, 8));
        realloced = true;
    }

    int copied = 0;
    for (int32_t i = 0; i < a->_size; ++i) {
        uint8_t* p = a->_data + static_cast<int64_t>(i) * 8;
        if (p == nullptr) continue;        // retail tests the COMPUTED address
        *reinterpret_cast<void**>(p) =
            *reinterpret_cast<void* const*>(s->_data + static_cast<int64_t>(i) * 8);
        ++copied;
    }

    if (RtDiffEnabled()) {
        RtDiffRegister(&g_j4_180003970.d);
        // The one INDEPENDENT thing available at a live call site: the copied
        // extent must equal the SOURCE buffer, which is not a restatement of
        // our own growth rule but a comparison against data we did not write.
        const bool ok = (a->_size == s->_size) &&
                        (a->_size == 0 ||
                         std::memcmp(a->_data, s->_data,
                                     static_cast<size_t>(a->_size) * 8) == 0);
        char o[128];
        std::snprintf(o, sizeof(o), "n=%d cap=%d copied=%d realloc=%d",
                      a->_size, a->_capacity, copied, realloced ? 1 : 0);
        RtDiffReport(&g_j4_180003970.d, ok, o, "prefix == src prefix");
    }
    return a;
}

// Cutover closure batch 2 (2026-09-12): 0x18001b1a0, `Array<Graphics::
// Geometry*>::operator=`. Read out of its own body and IDENTICAL in shape to
// 0x180003970 above -- dtor first (CALL 0x18001bf50, itself already ported as
// array.cpp's `J1_ENTRY(18001bf50)`, a parameterless body so which address it
// is does not matter), then the same realloc-and-copy loop, T a raw 8-byte
// pointer with no refcount and no pre-zero. A second occurrence of one J4
// body, not a new one — its own Site so the diff registry attributes
// divergences to the right retail address.
Site g_j4_18001b1a0 = { { "Collection::Array::operator= @0x18001b1a0", 0, 0, false, false },
                        0x18001b1a0 };
void* RtColl_ArrayAssign_18001b1a0(void* self, const void* src) {
    DProbeGate();
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    const ArrayHdr* s = static_cast<const ArrayHdr*>(src);
    if (a == s) return a;

    ArrayDtorImpl(a);                      // CALL 0x18001bf50 — a J1 body

    const int32_t newSize = s->_size;
    const int32_t oldSize = a->_size;      // always 0; dtor already zeroed it
    a->_size = newSize;

    bool realloced = false;
    if (newSize > a->_capacity) {
        a->_capacity = newSize;
        if (newSize == static_cast<int32_t>(oldSize + 1) && oldSize != 0)
            a->_capacity = static_cast<int32_t>(newSize * 3);   // dead; kept faithful
        a->_data = static_cast<uint8_t*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity)) * 8, 8));
        realloced = true;
    }

    int copied = 0;
    for (int32_t i = 0; i < a->_size; ++i) {
        uint8_t* p = a->_data + static_cast<int64_t>(i) * 8;
        if (p == nullptr) continue;
        *reinterpret_cast<void**>(p) =
            *reinterpret_cast<void* const*>(s->_data + static_cast<int64_t>(i) * 8);
        ++copied;
    }

    if (RtDiffEnabled()) {
        RtDiffRegister(&g_j4_18001b1a0.d);
        const bool ok = (a->_size == s->_size) &&
                        (a->_size == 0 ||
                         std::memcmp(a->_data, s->_data,
                                     static_cast<size_t>(a->_size) * 8) == 0);
        char o[128];
        std::snprintf(o, sizeof(o), "n=%d cap=%d copied=%d realloc=%d",
                      a->_size, a->_capacity, copied, realloced ? 1 : 0);
        RtDiffReport(&g_j4_18001b1a0.d, ok, o, "prefix == src prefix");
    }
    return a;
}

}  // namespace rtcoll

// ============================================================================
// Registration
// ============================================================================
//
// `RegisterRuntimeAddr` and not `RegisterRuntimeExport`, for lane Q's reason:
// none of these instantiations has an exported name in either build. Every
// address is spelled as a LITERAL at the call below even though the entry-point
// name already contains it, because tools/runtime_census.py counts address
// registrations by scanning for that literal
// (reference_census_cannot_see_nonliteral_registrations.md — twelve live ports
// went uncounted for a full wave when the address was a named constant).
//
// *** THIS REGISTRATION CHANGES WHAT LANE Q's OWN CODE CALLS. ***
// array.cpp reaches 0x1800059e0, 0x180052110 and 0x180005930 through
// `static Fn fn = ResolveAddr(Module::Runtime, …)`, and ResolveAddr consults the
// port registry FIRST. Those statics initialise on FIRST USE — a real Array
// call, long after any init — so registering here binds them to these bodies
// without any ordering requirement between the two inits. It does mean that
// from this commit, lane Q's three non-trivial specs no longer reach the DLL
// for their destroy step, and array.cpp's row table saying they do is corrected
// in the same change.
void RtCollectionDestroyPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_COLLECTION")) return;
    if (!RtWaveEnabled("BANISHED_RT_ARRAY_DESTROY")) return;

    using namespace rtcoll;

    // ---- the three element-destroy helpers our tree reaches ---------------
    // The other eight are ported and deliberately unregistered; the reason and
    // the full list are above the A_ENTRY block.
    RegisterRuntimeAddr(0x180052110, (void*)&RtColl_DestroyInner_180052110);
    RegisterRuntimeAddr(0x180005930, (void*)&RtColl_DestroyStrings_180005930);
    RegisterRuntimeAddr(0x1800059e0, (void*)&RtColl_DestroyStrings_1800059e0);
    // Wave 9 lane UIW: two more of "the eight", now that array.cpp's
    // kSpec_GraphSeries and kSpec_StringRef reach them by address. See the note
    // above the A_ENTRY block.
    RegisterRuntimeAddr(0x1800419b0, (void*)&RtColl_DestroyInner_1800419b0);
    RegisterRuntimeAddr(0x180011010, (void*)&RtColl_DestroyStrings_180011010);
    RegisterRuntimeAddr(0x18004fe20, (void*)&RtColl_DestroyStrings_18004fe20);
    // Wave 9 lane DESTROY: the packfile-entry range destroy. array.cpp's
    // kSpec_PkgEntry reaches it by address from ReadPackfile's SetSize, and it
    // was the attempt-17 cutover wall (an EXEC of 0 with the DLL absent).
    RegisterRuntimeAddr(0x180026b50, (void*)&RtColl_DestroyStrings_180026b50);
    // Wave 26 lane INT33: the fifth Body-B address. See the B_ENTRY line -- it
    // is one of "the eight", and it is registered now because array.cpp's
    // kSpec_ExtStr10at08 gives it the caller the banner says to wait for.
    RegisterRuntimeAddr(0x180039640, (void*)&RtColl_DestroyStrings_180039640);
    RegisterRuntimeAddr(0x180039580, (void*)&RtColl_DestroyStrings_180039580);
    // Wave 27 lane FRONTIER: the sixth Body-B address. array.cpp's
    // kSpec_ExtArrayElem (0x18002c6a0) reaches it by address on BOTH of its
    // destroy arms, and 0x180006b80 below reaches it as a direct call.
    RegisterRuntimeAddr(0x180006ac0, (void*)&RtColl_DestroyStrings_180006ac0);
    // Wave 27 lane FRONTIER: UI::StringTable's TextEntry range destroy. Two
    // callers, both ported in this commit - 0x180004a90 below and array.cpp's
    // kSpec_StringPair10 (0x180078c90).
    RegisterRuntimeAddr(0x180005570, (void*)&RtColl_DestroyStrings_180005570);
    // Cutover closure batch 2: Material parameter array + ShaderProgram blob
    // array element-destroy helpers. Both have callers registered just below.
    RegisterRuntimeAddr(0x180005880, (void*)&RtColl_DestroyStrings_180005880);
    RegisterRuntimeAddr(0x18004ff80, (void*)&RtColl_DestroyInner_18004ff80);

    // ---- the one element COPY-ASSIGN with no frontier at all ---------------
    // 37 instructions, zero callees. Its sibling 0x180051650 is NOT here: that
    // one calls Audio's 0x180050d30 and stays gated with the Wave 8 Audio
    // holdbacks, exactly where lane Q left it.
    RegisterRuntimeAddr(0x180004220, (void*)&RtColl_SpriteEntryAssign_180004220);

    // ---- J2: ~Array with an element-destroy prologue, six addresses --------
    RegisterRuntimeAddr(0x180010ab0, (void*)&RtColl_ArrayDtorElem_180010ab0);
    RegisterRuntimeAddr(0x18004fa30, (void*)&RtColl_ArrayDtorElem_18004fa30);
    RegisterRuntimeAddr(0x18001c7e0, (void*)&RtColl_ArrayDtorElem_18001c7e0);
    RegisterRuntimeAddr(0x180033880, (void*)&RtColl_ArrayDtorElem_180033880);
    RegisterRuntimeAddr(0x1800416d0, (void*)&RtColl_ArrayDtorElem_1800416d0);
    RegisterRuntimeAddr(0x18005bf60, (void*)&RtColl_ArrayDtorElem_18005bf60);
    RegisterRuntimeAddr(0x180073c90, (void*)&RtColl_ArrayDtorElem_180073c90);
    // Wave 25 lane INTERNAL42 — see the banner above these three for why one
    // opcode hash covers all three bodies and three specs do not.
    RegisterRuntimeAddr(0x18002ac40, (void*)&RtColl_ArrayDtorElem_18002ac40);
    RegisterRuntimeAddr(0x180039360, (void*)&RtColl_ArrayDtorElem_180039360);
    RegisterRuntimeAddr(0x180039280, (void*)&RtColl_ArrayDtorElem_180039280);
    // Wave 26 lane INT33 — ~Array<PackageFile::Entry>, PackageFile+0xB0. Its
    // helper 0x180026b50 is registered eight lines above, so this entry point
    // and the helper now BOTH route through this file; before it, retail's
    // 0x1800269a0 called retail's 0x180026b50 by rel32 and our registration of
    // the helper could never be reached from ~PackageFile.
    RegisterRuntimeAddr(0x1800269a0, (void*)&RtColl_ArrayDtorElem_1800269a0);
    // Wave 27 lane FRONTIER -- ~Array<Object::ExternalArrayElement>. Same
    // both-halves rule as the line above: its helper 0x180006ac0 is registered
    // in the block higher up, so at cutover this entry point and the helper are
    // both ours.
    RegisterRuntimeAddr(0x180006b80, (void*)&RtColl_ArrayDtorElem_180006b80);
    // Wave 27 lane FRONTIER -- ~Array<UI::StringTable::TextEntry>, the member
    // ??1StringTable@UI@@ destroys at this+0x20.
    RegisterRuntimeAddr(0x180004a90, (void*)&RtColl_ArrayDtorElem_180004a90);
    // Cutover closure batch 2 -- ~Array<MaterialParam> (~MaterialInstance) and
    // ~Array<Array<byte>> (~ShaderProgram / ~VertexProgram / ~PixelProgram).
    RegisterRuntimeAddr(0x180004c80, (void*)&RtColl_ArrayDtorElem_180004c80);
    RegisterRuntimeAddr(0x18004fbd0, (void*)&RtColl_ArrayDtorElem_18004fbd0);
    RegisterRuntimeAddr(0x180051fe0, (void*)&RtColl_ArrayDtorElem_180051fe0);

    // ---- J2b -------------------------------------------------------------
    RegisterRuntimeAddr(0x18001c8c0, (void*)&RtColl_ArrayDtorEhVec_18001c8c0);

    // ---- J4 (the half whose frontier is clean) ----------------------------
    RegisterRuntimeAddr(0x180003970, (void*)&RtColl_ArrayAssign_180003970);
    RegisterRuntimeAddr(0x18001b1a0, (void*)&RtColl_ArrayAssign_18001b1a0);

    // NOT registered, deliberately, and NOT because they are hard:
    //   the eight helpers listed above the A_ENTRY block, including:
    //   0x18005c550  — the recursive nested-pair destroy. It IS ported
    //                  (`DestroyNestedPair`, driven by the 0x18005bf60 entry
    //                  point exactly as retail calls it), but it gets no
    //                  registration of its own because nothing in our tree
    //                  reaches it by address, and a registration nothing calls
    //                  reports calls=0 forever — which is the signal this wave
    //                  uses to find DEAD ports
    //                  (reference_uncalled_port_is_not_verified.md). Register it
    //                  the day a caller appears.
    //   0x180010110  — the other J4. See the note above RtColl_ArrayAssign.

    rtcoll::g_dprobePending = RtCollectionDestroyProbeArmed();

    // A LANE-OWNED IDENTITY MARKER (the idiom lane PKG2 established in
    // package_file.cpp). This lane's gate for "the exe that RAN is the one I
    // built" is a grep of the run's OWN log for the line below, not an md5 of
    // what is on disk when I look -- four lanes this wave measured a build that
    // was not the one under test.
    //
    // It names 0x180026b50 explicitly rather than printing a COUNT of this
    // file's registrations, for PKG2's reason one level down: a count changes
    // whenever any other lane adds a row to this same init, so it would fire
    // falsely on a good run and silently pass on a bad one where two changes
    // cancelled. The address is what this lane landed, so the address is what
    // the gate keys on.
    printf("[RtPort]   Collection::Array destroy (lane DESTROY): "
                "0x180026b50 registered, built " __DATE__ " " __TIME__ "\n");
}
