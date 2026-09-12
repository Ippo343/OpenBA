// ============================================================================
// runtime/src/video/shader_program_names.cpp — WAVE 29 lane AVBODY
//
//   ?GetConstantName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z  0x18004e4b0
//   ?GetTextureName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z   0x18004e420
//
// These two are Video's ENTIRE remaining body surface. Re-derived on this tip,
// not taken from the kickoff:
//
//   $ python tools/runtime_census.py --unported --namespace Video
//     Called but not ported (20):
//       12 x ??_7*Data@Video@@6B@   UNPROMOTABLE(NO-OP)   -- not bodies
//        6 x ?_first*/_singleton@   UNPROMOTABLE(HARMFUL) -- not bodies
//        2 x  <-- these
//
// ============================================================================
// THE RETURN IS `System::String` BY VALUE => A HIDDEN RDX RETURN SLOT
// ============================================================================
// `?AV` in `?AVString@System@@` is the by-VALUE spelling. `AEBV` would be
// reference-to-const. The difference is the whole defect class: MSVC returns a
// non-trivially-copyable class through a caller-allocated slot passed in RDX,
// so the real register layout is
//
//     RCX = this, RDX = String* ret, R8D = index
//
// An `fn_*` typedef that omits the slot (`String (*)(void*, int32_t)`) puts the
// index in RDX and returns RAX as if it were the String. It COMPILES, and it
// RETURNS THE RIGHT ANSWER, because RAX comes back holding the slot address
// either way -- while having written 8 bytes through whatever the caller had in
// RDX. That is reference_hidden_retslot_wild_write.md, and it is the trap this
// lane was scoped around.
//
// TWO INDEPENDENT CONFIRMATIONS that the slot is real, neither of them "the
// mangling says so":
//   1. The bytes READ RDX three times: `MOV [RSP+0x10],RDX` (spill to home
//      space), `MOV [RDX],R9` (the store), `MOV RAX,RDX` (the return). A body
//      with no return slot never touches RDX.
//   2. Lane IOLANE's discriminator, applied to these two by that lane and by
//      this one independently: "does the body READ RDX at all?" It does.
//      IOLANE's own four bodies do NOT (two are a bare `LEA RAX,[RCX+0x58]`),
//      which is why the hidden-slot half of the shared subject is this lane's
//      alone. Recorded because the kickoff and both briefs described the
//      problem as shared, and MEASURED it is not.
//
// ============================================================================
// THE TWO BODIES DIFFER IN EXACTLY ONE BYTE, PROVED MECHANICALLY
// ============================================================================
// Not read by eye. Both extents are 0x44 bytes, derived independently of
// .pdata (first `CC CC` inter-function padding at +0x44 in both cases, which
// agrees with Ghidra's bound -- two derivations, kickoff warning 16), and
// neither body contains a single rel32 or RIP-relative field, so a RAW byte
// diff is SOUND here rather than reporting the ~23% noise of
// reference_position_only_diffs_hide_one_body.md. The precondition was checked
// by the script, not assumed.
//
//   A GetConstantName 48895424104883ec18...4803 41 30 4c890a488b08...
//   B GetTextureName  48895424104883ec18...4803 41 20 4c890a488b08...
//   differing byte offsets: 1
//     +0x23   A=0x30  B=0x20        <-- the disp8 of ADD RAX,[RCX+disp]
//
// So: SAME CODE over a DIFFERENT MEMBER ARRAY. `_constants` at +0x30,
// `_textures` at +0x20. Any second differing byte would have refuted that, and
// port-by-analogy is only legitimate because the diff was run.
//
// ============================================================================
// THE BODY, AND THE TWO WRITE WIDTHS THAT DIFFER
// ============================================================================
//   18004e4b0: MOV [RSP+0x10],RDX      ; ret slot -> home space (not observable)
//   18004e4b5: SUB RSP,0x18
//   18004e4b9: MOV [RSP+0x8],-0x2      ; MSVC EH frame marker (not reproduced)
//   18004e4c2: XOR R9D,R9D
//   18004e4c5: MOV [RSP],R9D           ; EH state 0 (not reproduced)
//   18004e4c9: MOVSXD RAX,R8D          ; *** SIGN-extended, see below
//   18004e4cc: SHL RAX,0x4             ; * 16 -- element stride
//   18004e4d0: ADD RAX,[RCX+0x30]      ; + this->_constants
//   18004e4d4: MOV [RDX],R9            ; *** QWORD store of 0, UNCONDITIONAL
//   18004e4d7: MOV RCX,[RAX]           ; element[index].name  (char*)
//   18004e4da: TEST RCX,RCX
//   18004e4dd: JZ  18004e4e5
//   18004e4df: MOV [RDX],RCX           ; *** QWORD store
//   18004e4e2: INC dword [RCX + -0x4]  ; *** DWORD increment -- NOT a qword
//   18004e4e5: MOV [RSP],0x1           ; EH state 1 (not reproduced)
//   18004e4ec: MOV RAX,RDX
//   18004e4ef: ADD RSP,0x18
//   18004e4f3: RET
//
// WRITTEN-OPERAND SET (kickoff §2.2 -- compared as a SET over a derived extent,
// which is how Camera::operator='s byte-vs-dword and EdgeMeshInstance's 1-of-19
// were both caught):
//     [RDX]      QWORD, written TWICE (0 then the pointer)
//     [RCX-0x4]  DWORD, written ONCE and only on the non-null path
// The widths DIFFER, and that is load-bearing in both directions:
//   * writing the refcount as a QWORD would clobber `chars[0..3]` of the
//     string payload, which lives at data+0 immediately after it;
//   * writing the out-slot as a DWORD would leave the high half of the
//     caller's String holding whatever was there before.
// runtime/types/system/String.h already spells the refcount `int32_t` and
// already provides StringRefCount(data) == data[-1] as an int32_t&, derived in
// an earlier wave from different functions. This body agrees with it, which is
// a second derivation rather than a restatement.
//
// THE UNCONDITIONAL `*ret = nullptr` IS NOT DEAD. It is the default-construct
// of the String temporary, and it is what makes the null-name case return the
// EMPTY string rather than leaving the caller's slot untouched. A port that
// only assigned on the non-null path would pass every test whose `ret` happened
// to start zeroed -- which is every test anyone would write by hand. The sweep
// below therefore pre-fills `ret` with a poison value on purpose.
//
// MOVSXD, NOT A ZERO EXTENSION. Ghidra's decompiled C prints a MOVSXD as an
// unsigned widening (reference_ghidra_c_output_vs_disassembly.md), so this is
// read off the DISASSEMBLY. A negative index walks BACKWARD from the array
// base in retail, and does so here. `(uint32_t)index * 16` would read at
// +0xFFFFFFF0 instead of -0x10 -- a 64 GB-away wild read that a non-negative
// sweep can never distinguish. Case 6 of the sweep is exactly that case.
//
// NO NULL CHECK ON `this` OR ON THE ARRAY BASE, and that is transcribed rather
// than "fixed" (CLAUDE.md: retail's unguarded derefs are load-bearing). The
// element's NAME is checked because retail checks it; nothing else is.
//
// ============================================================================
// VERIFICATION: AN INPUT SWEEP AGAINST THE SHIPPED EXPORT
// ============================================================================
// Both bodies are PURE apart from the refcount increment, and every input is
// memory this file can synthesise. So no gameplay route is needed: the sweep
// builds a ShaderProgram-shaped object and real String blocks, runs OURS and
// RETAIL'S on SEPARATE BUT IDENTICAL copies, and compares the full written set
// -- returned pointer, `ret->data`, AND the refcount of every block. That is
// project_pure_functions_verify_by_sweep.md's method, the one that verified
// GetAlignedPosition after it reported calls=0 in a 16.9M-call run.
//
// The retail side goes through ResolveExportRaw (GetProcAddress), NOT
// ResolveExport, because ResolveExport consults the port registry first and
// would hand back OUR function once registration lands -- turning the oracle
// into a self-check that agrees with itself forever
// (reference_shadow_marker_is_not_an_oracle.md).
//
// There IS a live route as well, and it already exists: Wave 28 lane GFXBODY
// ported `?GetConstantName@Material@Graphics@@` / `?GetTextureName@...`, whose
// whole body is a forwarder onto these two. So these get exercised by anything
// that reads a material's constant names. See this lane's doc for the measured
// attendance and for why GFXBODY's own counter could not be read.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../system/string.h"

namespace rtvidshadername {

using RtSystem::String;
using RtSystem::StringRefCount;

namespace {

// ---- attendance ------------------------------------------------------------
// Counted at the DOOR and, separately, at the BRANCH that does the work: a
// call count alone cannot say whether the non-null path ever ran, and
// reference_calls_zero_can_mean_always_took_the_uncovered_branch.md is this
// project's record of that exact confusion.
unsigned long long g_constCalls = 0;
unsigned long long g_constHits  = 0;   // BRANCH: name was non-null
unsigned long long g_texCalls   = 0;
unsigned long long g_texHits    = 0;   // BRANCH: name was non-null

// `MOVSXD RAX,R8D / SHL RAX,0x4 / ADD RAX,[RCX+disp] / MOV RCX,[RAX]`.
// int64_t, so the sign extension is the C-level MOVSXD.
inline char* NameAt(const void* self, int32_t index, unsigned disp) {
    uint8_t* const base =
        *reinterpret_cast<uint8_t* const*>(static_cast<const uint8_t*>(self) + disp);
    return *reinterpret_cast<char* const*>(base + static_cast<int64_t>(index) * 16);
}

}  // namespace

// ---- ?GetConstantName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z -------
extern "C" String* RtVideo_ShaderProgram_GetConstantName(const void* self,
                                                         String* ret,
                                                         int32_t index) {
    ++g_constCalls;
    ret->data = nullptr;                        // MOV [RDX],R9   -- QWORD, always
    char* const name = NameAt(self, index, 0x30);
    if (name) {                                 // TEST RCX,RCX / JZ
        ++g_constHits;
        ret->data = name;                       // MOV [RDX],RCX
        ++StringRefCount(name);                 // INC dword [RCX-0x4]
    }
    return ret;                                 // MOV RAX,RDX
}

// ---- ?GetTextureName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z --------
// The +0x20/+0x30 displacement is the ONLY difference; see the byte diff above.
extern "C" String* RtVideo_ShaderProgram_GetTextureName(const void* self,
                                                        String* ret,
                                                        int32_t index) {
    ++g_texCalls;
    ret->data = nullptr;
    char* const name = NameAt(self, index, 0x20);
    if (name) {
        ++g_texHits;
        ret->data = name;
        ++StringRefCount(name);
    }
    return ret;
}

// ============================================================================
// THE SWEEP
// ============================================================================

namespace {

using GetNameFn = String* (*)(const void*, String*, int32_t);

// A String block: {int32 length, int32 refCount, char chars[]}, `data` points
// at block+8 (runtime/types/system/String.h).
struct Block {
    int32_t length;
    int32_t refCount;
    char    chars[24];
};

// A ShaderProgram-shaped object: only +0x20 and +0x30 are ever read, and each
// points at an array of 16-byte records whose +0x00 is the char*. Nothing else
// in the object is touched, so nothing else is modelled -- and the padding is
// poisoned so that a port reading a field retail does not read gets a wild
// pointer rather than a plausible zero.
struct FakeProgram {
    uint8_t  poison[0x20];
    uint8_t* textures;    // +0x20
    uint8_t  pad28[8];
    uint8_t* constants;   // +0x30
};
static_assert(sizeof(FakeProgram) == 0x38, "FakeProgram layout");
static_assert(offsetof(FakeProgram, textures)  == 0x20, "textures @ +0x20");
static_assert(offsetof(FakeProgram, constants) == 0x30, "constants @ +0x30");

struct Fixture {
    Block    blocks[4];
    uint8_t  records[8 * 16];   // 8 records, +0x00 = char*
    FakeProgram prog;
};

// Element [i].name for a fixture, as a settable slot.
inline char** RecordName(Fixture& f, int i) {
    return reinterpret_cast<char**>(f.records + i * 16);
}

// Build an identical fixture on both sides. `nameMask` bit i => record i gets a
// real string; otherwise it is null.
void BuildFixture(Fixture& f, unsigned nameMask) {
    std::memset(&f, 0, sizeof(f));
    for (int i = 0; i < 4; ++i) {
        f.blocks[i].length   = 5;
        f.blocks[i].refCount = 1;
        std::snprintf(f.blocks[i].chars, sizeof(f.blocks[i].chars), "name%d", i);
    }
    std::memset(f.prog.poison, 0xAB, sizeof(f.prog.poison));
    std::memset(f.prog.pad28,  0xAB, sizeof(f.prog.pad28));
    for (int i = 0; i < 8; ++i) {
        *RecordName(f, i) = (nameMask & (1u << i)) ? f.blocks[i % 4].chars : nullptr;
    }
    // Records 2..5 are the "middle" of the array, so a base pointed at record 4
    // lets a NEGATIVE index walk backward into real memory.
    f.prog.constants = f.records + 4 * 16;
    f.prog.textures  = f.records + 4 * 16;
}

struct Outcome {
    String   ret;
    String*  returned;
    int32_t  refs[4];
    uint8_t  recordsAfter[8 * 16];
};

void RunOne(GetNameFn fn, unsigned nameMask, int32_t index, uint64_t retPoison,
            Outcome& out) {
    Fixture f;
    BuildFixture(f, nameMask);
    // Pre-fill the out-slot with POISON, not zero. The unconditional
    // `*ret = nullptr` is invisible to any case whose slot starts zeroed.
    std::memcpy(&out.ret, &retPoison, sizeof(out.ret));
    out.returned = fn(&f.prog, &out.ret, index);
    for (int i = 0; i < 4; ++i) out.refs[i] = f.blocks[i].refCount;
    std::memcpy(out.recordsAfter, f.records, sizeof(out.recordsAfter));
}

struct Case { const char* what; unsigned mask; int32_t index; uint64_t poison; };

// `which` selects the member array so the SAME case table exercises both
// bodies: 0 = constants (+0x30), 1 = textures (+0x20).
int SweepOne(const char* label, const char* mangled, GetNameFn ours,
             int& outCompared) {
    GetNameFn theirs = reinterpret_cast<GetNameFn>(ResolveExportRaw(mangled));
    if (!theirs) {
        printf("[AVBODY29]   %s: SKIPPED -- ResolveExportRaw returned null "
               "(a name the DLL does not contain resolves null SILENTLY, "
               "reference_resolveexport_phantom_names.md)\n", label);
        return -1;
    }

    static const Case kCases[] = {
        // The two ordinary paths.
        { "index 0, name present",        0xFFu, 0,  0x0000000000000000ull },
        { "index 0, name NULL",           0x00u, 0,  0x0000000000000000ull },
        // The unconditional-store case: slot starts POISONED and the name is
        // null, so a port that only assigns on the non-null path leaves the
        // poison behind and is caught here and ONLY here.
        { "index 0, name NULL, poisoned", 0x00u, 0,  0xDEADBEEFCAFEF00Dull },
        { "index 1, name present",        0xFFu, 1,  0xDEADBEEFCAFEF00Dull },
        { "index 3, mixed mask",          0x55u, 3,  0x1111111111111111ull },
        // MOVSXD: a negative index must walk BACKWARD from the base.
        { "index -1 (MOVSXD, backward)",  0xFFu, -1, 0x2222222222222222ull },
        { "index -4 (MOVSXD, backward)",  0x0Fu, -4, 0x3333333333333333ull },
        // Same index twice in one run, to show the refcount INCREMENTS rather
        // than being set to a constant.
        { "index 2, name present",        0xFFu, 2,  0x4444444444444444ull },
    };

    int diverged = 0;
    for (const Case& c : kCases) {
        Outcome a, b;
        RunOne(ours,   c.mask, c.index, c.poison, a);
        RunOne(theirs, c.mask, c.index, c.poison, b);
        ++outCompared;

        const bool sameRet  = (a.ret.data == b.ret.data);
        // The returned POINTER must be the slot we passed in, on both sides.
        const bool sameSlot = (a.returned == &a.ret) && (b.returned == &b.ret);
        bool sameRefs = true;
        for (int i = 0; i < 4; ++i) if (a.refs[i] != b.refs[i]) sameRefs = false;
        const bool sameRecords =
            std::memcmp(a.recordsAfter, b.recordsAfter, sizeof(a.recordsAfter)) == 0;

        if (!sameRet || !sameSlot || !sameRefs || !sameRecords) {
            ++diverged;
            // Name the FAILING TERM, not just "diverged": a predicate whose
            // verdict is an N-way AND must say which term failed
            // (reference_shadow_predicate_must_name_its_failing_term.md).
            printf("[AVBODY29]   %s DIVERGED on \"%s\": "
                   "retData ours=%p theirs=%p (%s) | slotReturned %s | "
                   "refs ours=[%d %d %d %d] theirs=[%d %d %d %d] (%s) | "
                   "records %s\n",
                   label, c.what, (void*)a.ret.data, (void*)b.ret.data,
                   sameRet ? "ok" : "MISMATCH",
                   sameSlot ? "ok" : "MISMATCH",
                   a.refs[0], a.refs[1], a.refs[2], a.refs[3],
                   b.refs[0], b.refs[1], b.refs[2], b.refs[3],
                   sameRefs ? "ok" : "MISMATCH",
                   sameRecords ? "ok" : "MISMATCH");
        }
    }
    return diverged;
}

// ---- the negative control --------------------------------------------------
// It must corrupt something the comparison ACTUALLY READS. A control that
// mutates a value nothing compares reports NOT CAUGHT and indicts a sound
// instrument (reference_control_must_pick_a_compared_subject.md). The three
// below each target ONE compared term, and they are scored SEPARATELY rather
// than as a union (reference_union_control_hides_a_blind_screen.md).

String* Ctl_NoRetSlotInit(const void* self, String* ret, int32_t index) {
    // Omits the unconditional `*ret = nullptr`. Targets: ret.data.
    char* const name = NameAt(self, index, 0x30);
    if (name) { ret->data = name; ++StringRefCount(name); }
    return ret;
}
String* Ctl_WrongElement(const void* self, String* ret, int32_t index) {
    // Reads the WRONG element (index^1), a bounded stand-in for any
    // index-arithmetic defect. Targets: ret.data.
    //
    // WHAT THIS CONTROL DELIBERATELY IS NOT, and the limit is worth stating:
    // the defect actually worth modelling here is MOVSXD read as a ZERO
    // extension, but with a negative index that computes base + 0xFFFFFFFF*16
    // -- a ~64 GB wild read. It would FAULT, not diverge, so the screen would
    // never get to report it and a "CAUGHT" would be a lie about the
    // mechanism. index^1 stays inside the fixture for every case in the table
    // (all indices are in [-4,3], and so are all of index^1), so it reports.
    // The sign-extension property itself is checked the honest way instead:
    // cases 6 and 7 use NEGATIVE indices and compare against RETAIL, which is
    // the oracle for that behaviour.
    ret->data = nullptr;
    char* const name = NameAt(self, index ^ 1, 0x30);
    if (name) { ret->data = name; ++StringRefCount(name); }
    return ret;
}
String* Ctl_NoRefcount(const void* self, String* ret, int32_t index) {
    // Skips the INC. Targets: refs[].
    ret->data = nullptr;
    char* const name = NameAt(self, index, 0x30);
    if (name) { ret->data = name; }
    return ret;
}

}  // namespace

// ---- the entry point -------------------------------------------------------
int Sweep() {
    printf("[AVBODY29] ==== ShaderProgram name-getter sweep ====\n");
    printf("[AVBODY29] SUBJECT: 2 bodies "
           "(?GetConstantName@ShaderProgram@Video@@ 0x18004e4b0, "
           "?GetTextureName@ShaderProgram@Video@@ 0x18004e420); "
           "ours vs ResolveExportRaw (the SHIPPED body, registry bypassed); "
           "8 cases each; compared terms = {ret.data, returned==&ret, "
           "4 refcounts, 128 record bytes}\n");

    int compared = 0;
    const int dc = SweepOne("GetConstantName",
        "?GetConstantName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z",
        &RtVideo_ShaderProgram_GetConstantName, compared);
    const int dt = SweepOne("GetTextureName",
        "?GetTextureName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z",
        &RtVideo_ShaderProgram_GetTextureName, compared);

    // ---- controls, scored one at a time --------------------------------
    // Each is run against RETAIL's GetConstantName over the same case table.
    // "caught" = at least one case diverged. A control that is NOT caught
    // means the screen is blind to that term, and says which term.
    printf("[AVBODY29] negative controls (each targets ONE compared term, "
           "scored SEPARATELY -- not as a union):\n");
    struct Ctl { const char* name; GetNameFn fn; const char* term; };
    static const Ctl kCtls[] = {
        { "omit the unconditional *ret=nullptr", &Ctl_NoRetSlotInit, "ret.data" },
        { "read the wrong element (index^1)",    &Ctl_WrongElement,  "ret.data" },
        { "omit the refcount INC",               &Ctl_NoRefcount,    "refcounts" },
    };
    int ctlCaught = 0;
    for (const Ctl& c : kCtls) {
        int n = 0;
        const int d = SweepOne("[control]",
            "?GetConstantName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z",
            c.fn, n);
        const bool caught = (d > 0);
        if (caught) ++ctlCaught;
        printf("[AVBODY29]   control \"%s\" (term %s): %s (%d/%d cases)\n",
               c.name, c.term, caught ? "CAUGHT" : "*** NOT CAUGHT -- SCREEN IS BLIND ***",
               d, n);
    }

    printf("[AVBODY29] RESULT: compared=%d divergences: GetConstantName=%d "
           "GetTextureName=%d ; controls caught %d/3\n",
           compared, dc, dt, ctlCaught);
    fflush(stdout);
    return (dc > 0 || dt > 0) ? 1 : 0;
}

// ---- registration ----------------------------------------------------------
// Each mangled name is its own STRING LITERAL. tools/runtime_census.py matches
// `RegisterRuntimeExport("<literal>"` and cannot see through a named constant
// (reference_census_cannot_see_nonliteral_registrations.md) -- twelve live
// ports were invisible to the board for that reason.
int Register() {
    int count = 0;
    RegisterRuntimeExport("?GetConstantName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z",
                          (void*)&RtVideo_ShaderProgram_GetConstantName); ++count;
    RegisterRuntimeExport("?GetTextureName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z",
                          (void*)&RtVideo_ShaderProgram_GetTextureName); ++count;
    return count;
}

// ---- attendance, printed UNCONDITIONALLY -----------------------------------
// Wave 28 defect #7 is that every lane's *Summary() is written and nothing
// calls it -- and it REPRODUCED on this lane's own subject:
// RtGraphicsWave28GfxbodySummary() has ZERO callers in the tree, so the
// g_getConstNameCalls counter that would have reported these two bodies'
// attendance through Material::GetConstantName never printed. This lane does
// not rely on an aggregator calling it: a static destructor runs it, the same
// mechanism that actually produced Wave 28's one measured figure (799 calls,
// audio_scene_update.cpp's UpdateReport).
//
// It prints even when every counter is 0, deliberately. A report guarded by
// `if (count)` makes "never ran" and "ran and counted zero" the same silence,
// which is the failure mode that made defect #7 invisible in the first place.
namespace {
struct NameGetterReport {
    ~NameGetterReport() {
        printf("[AVBODY29] ShaderProgram ATTENDANCE: "
               "GetConstantName calls=%llu (nameNonNull=%llu) ; "
               "GetTextureName calls=%llu (nameNonNull=%llu)\n",
               g_constCalls, g_constHits, g_texCalls, g_texHits);
        fflush(stdout);
    }
};
NameGetterReport g_nameGetterReport;
}  // namespace

}  // namespace rtvidshadername

// The two seams rt_video_port.cpp calls, at global scope with C++ linkage, the
// same shape as every other Video lane's (`extern int RtVideoBuffersRegister();`).
int  RtVideoShaderProgramNamesRegister() { return rtvidshadername::Register(); }
int  RtVideoShaderProgramNamesSweep()    { return rtvidshadername::Sweep(); }
