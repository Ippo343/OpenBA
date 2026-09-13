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
// The retail side goes through the raw DLL door (GetProcAddress), NOT
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
