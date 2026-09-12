// ============================================================================
// runtime/src/ui/spritesheet_new.cpp — Wave 24 lane SMALLBODY
//   ??2SpriteSheet@UI@@SAPEAX_K@Z    retail 0x180002e60  ord 895   99 bytes
// ============================================================================
//
// The `UI::SpriteSheet` class `operator new`. One of the nine rows in this
// lane's brief, and the only one of the nine that admits a TWO-SIDED oracle
// needing neither a game route nor a fabricated subject — see ORACLE below.
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED BEFORE A LINE WAS WRITTEN
// ---------------------------------------------------------------------------
// `reference_grep_for_the_port_before_writing_it.md` (three lanes in one wave
// re-ported what the tree already had) and
// `reference_half_registered_port_reads_as_absent.md` (a body ported but never
// REGISTERED is indistinguishable from an absent one to the census). So the
// grep was by ADDRESS *and* by NAME, scoped to the source roots:
//
//   grep -rn '??2SpriteSheet@UI@@SAPEAX' runtime/ code/ standalone/ engine/ dx11/
//     -> exactly ONE hit, and it is a CONSUMER:
//        runtime/src/save/saveload_draw.cpp:601
//           void* p = RT_EXPORT_ONCE(OpNewFn, "??2SpriteSheet@UI@@SAPEAX_K@Z")(0x58);
//   grep -rn '180002e60' runtime/ code/ standalone/ engine/ dx11/   -> NOTHING
//
// The greps were SCOPED to the source roots, not run at the repo root:
// `.claude/worktrees/` holds sibling lane checkouts and an unscoped grep
// returns a confident wrong number with no symptom (WAVE24_KICKOFF §1a, which
// measured that exact inflation at 22x).
//
// The one hit is also this port's ROUTE: `saveload_draw.cpp` builds the
// save/load screen's one-sprite sheet and asks for 0x58 bytes. So this row is
// not merely registerable — it is ENTERED, by our own tree, on the save/load
// path, which is what the attendance counter below reports.
//
// ---------------------------------------------------------------------------
// THE BODY, AND WHY IT IS NOT ASSUMED FROM ITS SIBLING
// ---------------------------------------------------------------------------
// This is instruction-for-instruction `??2ToolTipDialog@UI@@SAPEAX_K@Z`
// (0x18004cfd0), which runtime/src/ui/tooltipdialog.cpp already ports. That
// file's own banner carries the warning that made this lane check rather than
// copy:
//
//     "Reading one and assuming the other matches by resemblance is exactly how
//      this family has been got wrong before (runtime/src/video/context_ctor.cpp
//      records the same check, and found a sibling using align 4)."
//
// So the two were DISASSEMBLED SIDE BY SIDE for this lane, not compared by
// shape. Both are 99 bytes (0x180002e60..0x180002ec3 and
// 0x18004cfd0..0x18004d033), both end on the same `int3` pad, and every
// mnemonic and every immediate matches — including the one that matters,
// `MOV R8D,0x8`. The only bytes that differ are the rel32 and RIP-relative
// displacement fields, which encode a DISTANCE from the instruction and so MUST
// differ between two copies at different addresses
// (reference_position_only_diffs_hide_one_body.md).
//
//     PUSH RDI / SUB RSP,0x30 / MOV [RSP+0x20],-2       MSVC unwind marker
//     MOV RBX,RCX                                       save `bytes`
//     RDI = [0x1801fc718] + 0x640                       the allocator's lock
//     CALL [0x18014b390]                                EnterCriticalSection
//     MOV R8D,0x8 / MOV RDX,RBX / MOV RCX,[0x1801fc718]
//     CALL 0x1800c6e60                                  Allocate(alloc,bytes,8)
//     CALL [0x18014b388]                                LeaveCriticalSection
//     MOV RAX,RBX(=saved result) / RET
//
// The two KERNEL32 slots were named from the DLL's OWN import table, not
// guessed: 0x18014b390 is `KERNEL32.dll!EnterCriticalSection` and 0x18014b388
// is `KERNEL32.dll!LeaveCriticalSection`.
//
// Enter/Allocate/Leave around 0x1801fc718 IS
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) with the
// alignment frozen to the immediate 8 — the identity tooltipdialog.cpp proved
// instruction for instruction and context_ctor.cpp uses too. The LOCK is
// therefore NOT reproduced here: it lives INSIDE MemAlloc, which is the door
// being called. Reproducing it here as well would take the allocator's
// critical section twice.
//
// MemAlloc is reached BY ADDRESS because `ResolveAddr` consults the PORT
// REGISTRY FIRST (reference_resolveaddr_consults_port_registry_first.md), so
// when BANISHED_RT_ALLOC is armed this goes through our allocator body, and
// when it is not it falls through to the DLL. That is the same door
// tooltipdialog.cpp:184 and context_ctor.cpp already use, and it is what keeps
// this body from being the wrapper that calls a ported allocator DIRECTLY —
// the defect `reference_off_by_default_was_not_true.md` records, where an
// UNARMED run mutated retail's arena because one wrapper bypassed the door.
//
// ---------------------------------------------------------------------------
// ORACLE — A REAL TWO-SIDED COMPARISON, NOT A SHADOW SELF-CHECK
// ---------------------------------------------------------------------------
// PROBE SHAPE (WAVE24_KICKOFF §1c): **shadow/diff site exercised by someone
// else's run** — specifically an `RT_DIFF_SCALAR`, which CALLS THE SHIPPED
// BODY. It is deliberately NOT `RT_DIFF_SHADOW`: the ~250 shadow sites are
// SELF-CHECKS that take the port's own verdict and never consult the DLL, so a
// shared misunderstanding of retail passes every time
// (reference_shadow_marker_is_not_an_oracle.md). This lane has no game-
// directory lock, so the site is written to be exercised by lane DIFF's armed
// `BANISHED_RT_DIFF=1` sweep, which is the intended deliverable.
//
// THE PROBLEM A NAIVE ORACLE HITS: `operator new` returns a POINTER, and two
// calls necessarily return DIFFERENT pointers. Comparing the return values
// would fail 100% of calls and read like a catastrophic port
// (reference_oracle_must_not_compare_unwritten_bytes.md is the same family).
//
// WHAT IS COMPARED INSTEAD: the allocator's own bookkeeping DELTA across the
// call. `runtime/types/system/MemoryAllocator.h` pins two live counters in the
// allocator object:
//
//     +0x630  _allocatedBytes   int64   += the BLOCK size Allocate took
//     +0x638  _allocCount       int32   ++ per live block
//
// Both are written by `Allocate` and by nothing else on this path. So for a
// given `bytes`, ours and the shipped body must produce the SAME pair of
// deltas — and a delta is position-independent, which is exactly why it
// survives the fact that the pointers differ. The probe:
//
//     snapshot (_allocCount, _allocatedBytes)  ->  call OURS      -> delta A
//     snapshot (_allocCount, _allocatedBytes)  ->  call SHIPPED   -> delta B
//     RT_DIFF_SCALAR(A, B)
//     free the SHIPPED probe block so the arena is left as it was found
//
// The shipped body is reached with `RtOriginal`, NEVER `ResolveExport`: once
// this port registers, ResolveExport would hand back OUR function and the
// oracle would compare ours to ours and always agree — a probe whose ours-side
// reaches the body by a port-aware door compares retail with ITSELF, and one
// such Goal::Serialize probe was dark for five waves
// (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md).
// It sits inside the diff guard, so a normal run never pays the GetProcAddress
// (reference_rtoriginal_is_getprocaddress.md: two such calls on a hot path cost
// 5.6 points of sim time).
//
// EFFECTIVENESS COUNTER (KICKOFF §3 rule 3 — a green control can leave most of
// an instrument untested, so carry a count of how many cases CHANGED
// ANYTHING): `g_probePairs` counts the pairs where BOTH deltas were actually
// obtained, and `g_probeSkipped` counts the calls where they were not (the
// allocator singleton was null, the shipped body did not resolve, or the probe
// budget was spent). A `pairs=0` report means the oracle is DEAD, and says so
// rather than printing a clean `divergences=0`.
//
// NEGATIVE CONTROL (KICKOFF §3 rule 2 — a control must corrupt a subject the
// probe actually COMPARES, and must not corrupt one it should not):
// `BANISHED_RT_SPRITESHEETNEW_CONTROL=1` makes OUR body request `bytes + 0x20`.
//
//   * It corrupts something COMPARED: `_allocatedBytes` is in the compared
//     pair, and 0x20 more bytes cannot land in the same block-size bucket for
//     the size retail actually asks for (0x58 -> 0x78), so delta A must differ
//     from delta B and the site must report a divergence.
//   * It does NOT corrupt something that should NOT be compared: the returned
//     POINTER is deliberately outside the compared set, and the control leaves
//     the comparison's shape alone. `reference_a_control_can_corrupt_a_byte_
//     that_should_not_be_compared.md` is the mirror failure this avoids.
//   * `align` was REJECTED as a control knob after being checked rather than
//     assumed: `Allocate` computes `align8 = (align + 7) & ~7`, so passing 4
//     instead of 8 yields the SAME align8 and changes NOTHING the probe
//     compares. A control on it would have reported NOT CAUGHT and indicted a
//     sound instrument (reference_control_must_pick_a_compared_subject.md).
//
// ---------------------------------------------------------------------------
// ONE KILL SWITCH: BANISHED_RT_UISPRITESHEETNEW
// ---------------------------------------------------------------------------
// Its own switch rather than riding BANISHED_RT_UITOOLTIPDLG, so an A/B can
// move this row without moving ToolTipDialog's five bodies.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// ATTENDANCE — entries, and NOT a verification of anything.
// ---------------------------------------------------------------------------
// Counted unconditionally on every build, exactly as tooltipdialog.cpp does and
// for the reason it states: an RtDiff site increments `calls` only from inside a
// comparison, so a body that ran but never compared prints `calls=0`, which
// reads as "never ran" and means the opposite
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md). A green
// readiness screen is not a route (reference_a_green_readiness_screen_is_not_a_
// route.md); a zero here means the row is registered and DEAD.
uint64_t g_newEntries  = 0;
uint64_t g_newBytes    = 0;   // control: the size retail's caller asks for (0x58)
uint64_t g_newNull     = 0;   // control: the allocator refused
uint64_t g_probePairs  = 0;   // EFFECTIVENESS: comparisons that actually happened
uint64_t g_probeSkipped = 0;  // EFFECTIVENESS: armed calls that could NOT compare

struct SpriteSheetNewSummary {
    ~SpriteSheetNewSummary() {
        if (g_newEntries == 0) {
            printf("[RtPort] UI::SpriteSheet::operator new ATTENDANCE: not "
                   "entered in this run.\n");
            fflush(stdout);
            return;
        }
        printf("[RtPort] UI::SpriteSheet::operator new ATTENDANCE (entries, "
               "not comparisons): entries=%llu lastBytes=%llu nulls=%llu | "
               "ORACLE probePairs=%llu probeSkipped=%llu%s\n",
               (unsigned long long)g_newEntries,
               (unsigned long long)g_newBytes,
               (unsigned long long)g_newNull,
               (unsigned long long)g_probePairs,
               (unsigned long long)g_probeSkipped,
               (g_probePairs == 0)
                   ? "  <-- ORACLE DEAD: zero comparisons, so a clean "
                     "divergences=0 on this site means NOTHING"
                   : "");
        fflush(stdout);
    }
};
SpriteSheetNewSummary g_summary;

// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`, 0x1800c7a50. See the banner:
// the Enter/Allocate/Leave triple this body open-codes IS this function with the
// alignment frozen to 8. Reached BY ADDRESS so the port registry gets first
// refusal.
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`, 0x1800c7ad0 — the matching
// lock/Free/unlock triple, the address standalone/filename_runtime_hook.cpp:496
// already uses. ONLY used to return the oracle's own probe block; nothing on the
// normal path calls it.
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn) fn(p);
}

// The two live allocator counters the oracle reads, at
// `*(0x1801fc718) + 0x630 / +0x638` (runtime/types/system/MemoryAllocator.h).
// Read through the port-aware DATA door so a minted run and a DLL run both land
// on whatever storage the allocator actually published into.
constexpr int kAllocatedBytesOffset = 0x630;
constexpr int kAllocCountOffset     = 0x638;

uint8_t* AllocatorObject() {
    void** slot = RT_DATA_SLOT(void**,
                               "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
    return slot ? static_cast<uint8_t*>(*slot) : nullptr;
}

struct AllocStats {
    int64_t bytes;
    int32_t count;
};

bool ReadAllocStats(AllocStats* out) {
    uint8_t* a = AllocatorObject();
    if (!a) return false;
    out->bytes = *reinterpret_cast<const int64_t*>(a + kAllocatedBytesOffset);
    out->count = *reinterpret_cast<const int32_t*>(a + kAllocCountOffset);
    return true;
}

// The probe allocates a REAL block per comparison and frees it again. Capped so
// an armed sweep over a long run cannot turn the oracle into a memory workload;
// 64 pairs is far more than enough to separate "agrees" from "diverges", and the
// effectiveness counter reports how many were actually spent.
constexpr uint64_t kProbeBudget = 64;

bool ControlArmed() {
    static const int v = []() {
        const char* e = std::getenv("BANISHED_RT_SPRITESHEETNEW_CONTROL");
        return (e && e[0] && e[0] != '0') ? 1 : 0;
    }();
    return v != 0;
}

}  // namespace

// ============================================================================
// ??2SpriteSheet@UI@@SAPEAX_K@Z                            retail 0x180002e60
// ============================================================================
extern "C" void* RtUi_SpriteSheet_OperatorNew(uint64_t bytes) {
    RT_DIFF_BEGIN("UI::SpriteSheet::operator new (allocator bookkeeping delta)");

    ++g_newEntries;
    g_newBytes = bytes;                       // control: retail's caller passes 0x58

    // ---- the body: MOV R8D,0x8 ; CALL 0x1800c6e60, via MemAlloc -------------
    const uint64_t request = ControlArmed() ? (bytes + 0x20) : bytes;

    AllocStats before{}, after{};
    const bool haveBefore = rt_diff_on__ && ReadAllocStats(&before);

    void* p = MemAlloc(request, 8);           // the whole shipped body
    if (p == nullptr) ++g_newNull;

    // ---- the oracle --------------------------------------------------------
    if (rt_diff_on__) {
        bool compared = false;
        if (haveBefore && ReadAllocStats(&after) &&
            g_probePairs + g_probeSkipped < kProbeBudget) {
            const int64_t oursBytes = after.bytes - before.bytes;
            const int32_t oursCount = after.count - before.count;

            // The SHIPPED body, by RtOriginal — never ResolveExport, which would
            // now hand back this very function.
            using OpNewFn = void* (*)(uint64_t);
            OpNewFn theirs = RT_ORIGINAL_ONCE(OpNewFn, "??2SpriteSheet@UI@@SAPEAX_K@Z");
            AllocStats tb{}, ta{};
            if (theirs && ReadAllocStats(&tb)) {
                void* q = theirs(bytes);      // the UNCORRUPTED size, always
                if (ReadAllocStats(&ta)) {
                    const int64_t theirsBytes = ta.bytes - tb.bytes;
                    const int32_t theirsCount = ta.count - tb.count;
                    ++g_probePairs;
                    compared = true;
                    RT_DIFF_SCALAR(oursCount, theirsCount);
                    RT_DIFF_SCALAR(oursBytes, theirsBytes);
                }
                // Leave the arena as we found it. Freeing AFTER the delta is
                // taken, so the free does not enter the compared quantity.
                if (q) MemFree(q);
            }
        }
        if (!compared) ++g_probeSkipped;
    }

    return p;
}

// ============================================================================
// UI leftovers batch (docs/runtime_port/closure/WORKLIST.md) — ~SpriteSheet
// and its class's three per-TU `operator delete` copies.
// ============================================================================

// The exported `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` wrapper, MemFree
// above, is `static`-cached per translation unit already; these three rows
// all reuse that one cache rather than each keeping their own, since it is
// the SAME shipped body (0x1800c7ad0) underneath every one of them.

// `??3SpriteSheet@UI@@SAXPEAX@Z` (retail 0x180002ee0, 93 bytes) — decompiled:
// `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }`, i.e. exactly
// MemFree with the null test open-coded rather than delegated. Byte-identical
// in shape to the ten other per-TU copies runtime/src/object/deleting_dtor.cpp
// already catalogued.
extern "C" void RtUi_SpriteSheet_OperatorDelete(void* p) { MemFree(p); }
// `??3ToolTipDialog@UI@@SAXPEAX@Z` (retail 0x18004d050, 93 bytes). Same body.
extern "C" void RtUi_ToolTipDialog_OperatorDelete(void* p) { MemFree(p); }
// `??3DialogInstance@UI@@SAXPEAX@Z` (retail 0x180045490, 93 bytes). Same body.
extern "C" void RtUi_DialogInstance_OperatorDelete(void* p) { MemFree(p); }

// `??1SpriteSheet@UI@@UEAA@XZ` — retail 0x18011d670, ord 686.
// Releases the two refcounted String slots at +0x40 (`m_groupName`) then
// +0x38 (`m_textureName`) — in THAT order, per the disassembly — destroys the
// `m_sprites` array (+0x20, Array<SpriteInfo>) through the already-ported
// `~Array<SpriteEntry:0x38>` at 0x180004f50
// (runtime/src/collection/array_destroy.cpp), then tail-calls
// `Object::External::~External`. The vtable is re-stamped first, exactly as
// retail's `*(undefined***)this = &_vftable_` does — this class's own vtable,
// not a base class's, because a derived class's dtor thunk has already
// re-stamped ITS vtable and this call is the base-class step of that chain.
extern "C" void RtUi_SpriteSheet_Dtor(void* self) {
    uint8_t* p = static_cast<uint8_t*>(self);
    void* vtbl = RT_DATA_SLOT(void*, "??_7SpriteSheet@UI@@6B@");
    if (vtbl) *reinterpret_cast<void**>(p) = vtbl;

    auto release = [](uint8_t* slot) {
        uint8_t* str = *reinterpret_cast<uint8_t**>(slot);
        if (!str) return;
        int32_t* rc = reinterpret_cast<int32_t*>(str - 4);
        if (--*rc == 0) {
            uint8_t* blk = str - 8;
            *reinterpret_cast<uint8_t**>(slot) = blk;
            if (blk) MemFree(blk);
        } else {
            *reinterpret_cast<uint8_t**>(slot) = nullptr;
        }
    };
    release(p + 0x40);   // m_groupName
    release(p + 0x38);   // m_textureName

    using ArrayDtorFn = void* (*)(void*);
    static ArrayDtorFn arrayDtor = nullptr;
    if (!arrayDtor) arrayDtor = reinterpret_cast<ArrayDtorFn>(
        ResolveAddr(Module::Runtime, 0x180004f50));
    arrayDtor(p + 0x20);   // m_sprites, Array<SpriteInfo>

    using ExternalDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExternalDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ============================================================================
// Registration. BOTH doors — BY NAME and BY ADDRESS.
// ============================================================================
// The address door is not redundant with the name door: a caller that reaches a
// body by address silently keeps using the DLL when only the name is registered
// (reference_bypassed_port_needs_a_source_checker.md). Both are spelled as
// LITERALS so tools/runtime_census.py can see them
// (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
int RtUiSpriteSheetNewRegister() {
    if (!RtWaveEnabled("BANISHED_RT_UISPRITESHEETNEW")) {
        printf("[RtPort] BANISHED_RT_UISPRITESHEETNEW=0 -- "
               "UI::SpriteSheet::operator new left on the DLL\n");
        return 0;
    }
    RegisterRuntimeExport("??2SpriteSheet@UI@@SAPEAX_K@Z",
                          (void*)&RtUi_SpriteSheet_OperatorNew);
    RegisterRuntimeAddr(0x180002e60, (void*)&RtUi_SpriteSheet_OperatorNew);

    RegisterRuntimeExport("??1SpriteSheet@UI@@UEAA@XZ", (void*)&RtUi_SpriteSheet_Dtor);
    RegisterRuntimeAddr(0x18011d670, (void*)&RtUi_SpriteSheet_Dtor);

    // Three deleting-dtor rows (operator_delete), UI leftovers batch. Each is
    // one of the per-translation-unit `operator delete` copies
    // runtime/src/object/deleting_dtor.cpp already catalogued (byte-identical
    // to the exported `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`), reached
    // here by whichever class's scalar/vector deleting destructor calls it.
    RegisterRuntimeExport("??3SpriteSheet@UI@@SAXPEAX@Z", (void*)&RtUi_SpriteSheet_OperatorDelete);
    RegisterRuntimeAddr(0x180002ee0, (void*)&RtUi_SpriteSheet_OperatorDelete);

    RegisterRuntimeExport("??3ToolTipDialog@UI@@SAXPEAX@Z", (void*)&RtUi_ToolTipDialog_OperatorDelete);
    RegisterRuntimeAddr(0x18004d050, (void*)&RtUi_ToolTipDialog_OperatorDelete);

    RegisterRuntimeExport("??3DialogInstance@UI@@SAXPEAX@Z", (void*)&RtUi_DialogInstance_OperatorDelete);
    RegisterRuntimeAddr(0x180045490, (void*)&RtUi_DialogInstance_OperatorDelete);

    return 1;
}
