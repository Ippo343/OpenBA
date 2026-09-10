// ============================================================================
// runtime/src/object/external_compiled.cpp — Wave 11 lane OBJEXT
//   `Object::ExternalFactory::GetCompiledPath` and `Object::ExternalFactory::
//   FindOrLoad`.
// ============================================================================
//
//   ?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z
//                                                                  0x1800bec10
//   ?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z
//                                                                  0x1800bf320
//
// These two are in one file because they are one algorithm twice: the six-term
// concatenation that turns a resource path into the path of its COMPILED form
// under the data directory appears verbatim in both, and MSVC emitted it twice
// rather than making `FindOrLoad` call `GetCompiledPath`. Instruction for
// instruction, 0x1800bf398-0x1800bf44e and 0x1800bec37-0x1800becf1 are the same
// sequence with the same two literals in the same order. Splitting them across
// two files would put one lane's fix one file away from the other's copy.
//
// THE CONCATENATION (both bodies)
//
//     result = dataDir + dir + fname + "_" + ident + ".crs"
//
// where dir/fname/ident are `GetPathDirectory()`, `GetPathFilename(strip=true)`
// and `GetPathIdentifer()` of the input, `dataDir` is
// `IO::FileSystem::_singleton->GetDataDirectory()`, and the two literals live at
// 0x180155b04 ("_") and 0x180155afc (".crs"). `AdjustDelimiters` runs once over
// the finished string.
//
// A DEAD CALL THAT IS FAITHFULLY KEPT. Both bodies open with
// `path.GetPathExtension()` into a stack slot that is then never read — only
// released at the end (0x1800bec41 writes [RBP-0x18], 0x1800bee6f releases it,
// nothing in between touches it). It is dead in the sense that its VALUE is
// unused; it is not dead in the sense that it allocates and frees a String. It
// is transcribed rather than elided, because "this call has no effect" is
// exactly the kind of judgement this port has been wrong about before, and
// because eliding it would make the two bodies' allocation traffic differ from
// retail's for no gain.
//
// WHAT THIS PORT CLOSES
//
// `runtime/src/engine/mainloop_video.cpp`'s banner records that porting
// `OnVideoRestart` deliberately OPENED three unsatisfied names, two of which
// are `+Load@External` and `+GetCompiledPath@ExternalFactory`. This file closes
// the second. The first is HELD BACK — see docs/runtime_port/WAVE11_LANE_OBJEXT.md.
//
// EVIDENCE. Transcribed from `disassemble_function(address=)` on Runtime-x64.dll
// (image base 0x180000000, read 2026-09-09). Ghidra's C for both drops the
// right-hand operand of all five `System::operator+` calls and, for
// `FindOrLoad`, drops `ExpandRelativePath`'s third and fourth arguments
// entirely — the listing at 0x1800bf34f-0x1800bf359 shows `R8 = path` and
// `R9B = 1`, neither of which appears in the decompiled call
// (reference_ghidra_c_output_vs_disassembly.md).
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "external_path.h"
#include "external_compiled.h"
#include "external_path_stack.h"

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/setupdir_helpers.h"

namespace RtSystem {
Filename* RtSystem_Filename_CtorCStr(Filename* self, const char* src);           // 0x1800c6570
// Declared locally, house style — filename.cpp exposes no header of its own.
void      RtSystem_Filename_AdjustDelimiters(Filename* self);              // 0x1800c6970
Filename* RtSystem_Filename_GetPathFilename(const Filename* self, Filename* ret,
                                            bool stripExtension);          // 0x1800c6740
Filename* RtSystem_Filename_GetPathIdentifer(const Filename* self, Filename* ret);
                                                                           // 0x1800c6610
}  // namespace RtSystem

namespace RtIO {
// runtime/src/io/filesystem.cpp:185. Takes the FileSystem as void* there.
RtSystem::Filename* RtIo_FileSystem_GetDataDirectory(const void* fsVoid,
                                                     RtSystem::Filename* ret);
}  // namespace RtIO

namespace RtObject {

using RtSystem::Filename;
using RtSystem::String;
using RtSystem::RtString_AdoptInline;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtSystem_String_Concat;
using RtSystem::RtSystem_String_ConcatCStr;
using RtSystem::RtSystem_Filename_AdjustDelimiters;
using RtSystem::RtSystem_Filename_GetPathFilename;
using RtSystem::RtSystem_Filename_GetPathIdentifer;
using RtSystem::RtSystem_Filename_GetPathDirectory;

CompiledCounters g_cc = {};

namespace {

inline String*       AsStr(Filename* f)       { return reinterpret_cast<String*>(f); }
inline const String* AsStr(const Filename* f) { return reinterpret_cast<const String*>(f); }

// The int32 character count at data-8 (runtime/types/system/String.h). This is
// the field FindOrLoad's `CMP dword ptr [RAX + -0x8],0x0` at 0x1800bf345 reads,
// NOT the refcount at data-4.
inline int32_t BufLength(const char* d) {
    return reinterpret_cast<const int32_t*>(d)[-2];
}

// 0x180155b04 and 0x180155afc. Named so the negative controls can swap them.
const char* const kUnderscore = "_";
const char* const kCrs        = ".crs";

// `?_singleton@FileSystem@IO@@0PEAV12@EA`, retail 0x1801fc6a8 — the qword the
// `MOV RCX,qword ptr [0x1801fc6a8]` at 0x1800bec75 loads. Through the
// port-aware data door so it follows the mint at cutover.
inline const void* FileSystemSingleton() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

// ---------------------------------------------------------------------------
// The negative control. `0` is the real body.
// `BANISHED_RT_COMPILED_NEGCTL=<n>` forces one for a live run.
//   1  join with "-" instead of "_"
//   2  ".CRS" instead of ".crs"                (case: AdjustDelimiters must not hide it)
//   3  dir and fname swapped in the concatenation
//   4  GetPathFilename called with strip=false
//   5  the extension, not the identifier, in the "_<ident>" slot
// ---------------------------------------------------------------------------
int g_ccNegCtl = 0;

// ---------------------------------------------------------------------------
// THE SHARED CONCATENATION.
//
// Written once and called from both bodies BECAUSE THEY ARE THE SAME BYTES, and
// that claim is the comparison, not an assumption: the two instruction runs
// were diffed (0x1800bec37..0x1800becf1 against 0x1800bf398..0x1800bf44e) and
// differ only in which stack slots they name and in `FindOrLoad` writing its
// result to a slot rather than to the hidden return parameter. Where two
// retail bodies genuinely share bytes this port records the comparison rather
// than assuming it (external_set.h sets that precedent, and gives the reason:
// this family has produced four near-misses that differed only in an alignment
// immediate or a shift).
//
// `out` is raw on entry in retail (a stack slot) and is written before it is
// read, so callers pass an uninitialised `Filename`.
// ---------------------------------------------------------------------------
void BuildCompiledPath(const Filename* path, Filename* out) {
    ++g_cc.buildCalls;

    Filename ext{nullptr};                                       // [RBP-0x18]
    RtSystem_Filename_GetPathExtension(path, &ext);              // value UNUSED, see banner

    Filename ident{nullptr};                                     // [RBP-0x50]
    RtSystem_Filename_GetPathIdentifer(path, &ident);
    Filename fname{nullptr};                                     // [RBP+0x38]
    RtSystem_Filename_GetPathFilename(path, &fname, g_ccNegCtl != 4);
    Filename dir{nullptr};                                       // [RBP+0x30]
    RtSystem_Filename_GetPathDirectory(path, &dir);

    Filename dataDir{nullptr};                                   // [RBP-0x28]
    RtIO::RtIo_FileSystem_GetDataDirectory(FileSystemSingleton(), &dataDir);

    String t1{nullptr}, t2{nullptr}, t3{nullptr}, t4{nullptr}, t5{nullptr};
    RtSystem_String_Concat(&t1, AsStr(&dataDir),
                           AsStr(g_ccNegCtl == 3 ? &fname : &dir));
    RtSystem_String_Concat(&t2, &t1,
                           AsStr(g_ccNegCtl == 3 ? &dir : &fname));
    RtSystem_String_ConcatCStr(&t3, &t2, g_ccNegCtl == 1 ? "-" : kUnderscore);
    RtSystem_String_Concat(&t4, &t3, AsStr(g_ccNegCtl == 5 ? &ext : &ident));
    RtSystem_String_ConcatCStr(&t5, &t4, g_ccNegCtl == 2 ? ".CRS" : kCrs);

    out->data = nullptr;                                         // 0x1800becdd
    RtString_AdoptInline(AsStr(out), t5.data);                   // 0x1800bece0-0x1800beceb
    RtSystem_Filename_AdjustDelimiters(out);                     // 0x1800becf1

    // Retail's release order: reversed construction order.
    RtString_ReleaseInline(&t5);
    RtString_ReleaseInline(&t4);
    RtString_ReleaseInline(&t3);
    RtString_ReleaseInline(&t2);
    RtString_ReleaseInline(&t1);
    RtString_ReleaseInline(AsStr(&dataDir));
    RtString_ReleaseInline(AsStr(&dir));
    RtString_ReleaseInline(AsStr(&fname));
    RtString_ReleaseInline(AsStr(&ident));
    RtString_ReleaseInline(AsStr(&ext));
}


// ---------------------------------------------------------------------------
// THE INPUT SWEEP — GetCompiledPath's negative controls, all in ONE process.
//
// `GetCompiledPath` is PURE, and for a pure function an input sweep against the
// shipped export beats waiting for a caller
// (project_pure_functions_verify_by_sweep.md). It is also how every one of its
// negative controls gets scored without costing a game launch each, which
// matters because three lanes are queueing on one game directory this wave and
// a blocking acquire has already timed out once.
//
// The sweep runs on a REAL live path plus a fixed set that exercises the shapes
// the six terms can take: no directory, a nested directory, a path with an
// explicit `:identifier`, and one with no extension. Each control is scored
// SEPARATELY with the mechanism of any miss named.
//
// Every case is compared ours-vs-theirs as TEXT. Ours and theirs write two
// different fresh slots, so they cannot alias.
// ---------------------------------------------------------------------------
//
// The set is chosen so that every one of the six concatenated terms is
// EXERCISED BY MORE THAN ONE CASE where that is possible. A control that
// catches on 1 of N is one input away from reporting NOT CAUGHT and indicting
// a sound instrument, and its catch-rate is a coverage statement about this
// list rather than a statement about the control
// (reference_control_must_pick_a_compared_subject.md,
//  reference_uniform_population_is_not_a_verification.md).
//
// The `sens=` mask printed beside each control names exactly which entries are
// sensitive to it, so a future reader does not have to infer coverage from a
// ratio.
const char* const kSweepPaths[] = {
    /*  0 */ "Terrain/Grass.rsc",
    /*  1 */ "House.rsc",                        // EMPTY directory term
    /*  2 */ "Template/Building/Deep/Nested/Thing.rsc",
    /*  3 */ "Dialog/MainMenu.rsc:buttonOk",     // explicit :identifier
    /*  4 */ "Audio/Theme",                      // NO extension -> EMPTY filename
    /*  5 */ "Audio/Music/Theme.ogg",            // non-.rsc extension
    /*  6 */ "Terrain/Grass.rsc:alt",            // identifier AND extension
    /*  7 */ "Deep/Nested/Thing.tga:sprite",     // both, non-.rsc
    /*  8 */ "Solo.tga",                         // no dir, has ext
    /*  9 */ "A/B.c:d",                          // all four terms, minimal
    /* 10 */ "Solo",                             // no dir, NO ext
    /* 11 */ "Deep/Nested/NoExt",                // dir, NO ext
    /* 12 */ "A/B:c",                            // identifier, NO ext
};

// ---------------------------------------------------------------------------
// THE PREDICTED SENSITIVITY MASKS — WRITTEN BEFORE THE RUN, ON PURPOSE.
//
// A measured mask alone is a prettier ratio: without a prior to disagree with,
// whatever comes back reads as explicable, which is exactly how this lane's
// first sweep got past me. `ctl=4` caught on 1 of 6 where my model said 4-5 and
// I recorded it as "coverage" rather than chasing it.
//
// Chasing it found a FACT ABOUT THE FUNCTION, not about the input list.
// `?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z` (0x1800c6740) does NOT
// mean "strip the extension". Its backward scan for '.' runs unconditionally
// and `length = dotIndex - 1` strips the extension WHETHER OR NOT the flag is
// set. The flag is read in exactly one place:
//
//     if (dotIndex < 1) {            // no '.' at all, or one at index 0
//         if (stripExt) { *retval = 0; return retval; }   // EMPTY
//     }                              // else: fall through, keep the whole name
//
// So the flag means "yield an EMPTY filename when the path has no extension".
// For every path that HAS an extension it is dead. That single fact explains
// all five of the first sweep's ratios exactly, and it is corroborated by lane
// RRES's own counter `filenameFromStack // filename was empty -> taken from the
// stack`, which exists precisely because this call returns empty for
// extension-less paths.
//
// *** runtime/src/object/external_path_win.cpp DECLARES THIS PARAMETER
// `bool stripExtension`, AND THAT NAME IS WRONG. *** It is another lane's file
// so it is not renamed here; it is reported in WAVE11_LANE_OBJEXT.md §11.
//
// THE PRIOR IS A MODEL, NOT A STRING.
//
// It used to be six hand-written mask literals. One of them contradicted its
// own derivation: the comment beside it enumerated SIX insensitive indices and
// the mask carried SEVEN dots, and the extra dot was the very index that later
// "disagreed" with the run. The honest verdict there was `model right,
// transcription wrong` -- but "I meant to write X" after seeing X is
// unfalsifiable, and the only reason it was defensible at all is that the
// contradiction predated the run and could be pointed at.
//
// A hand-written mask cannot be made trustworthy by being more careful with it.
// So the mask is now COMPUTED from the input string by the predicates below,
// which ARE the model. There is one hand-written artefact instead of two that
// can disagree, a transcription slip is not expressible, and any disagreement
// the run reports is necessarily a MODEL error -- which is the only kind worth
// reading.
//
// It also extends to the live path for free: index kSweepN is predicted by
// running the same predicates over whatever string the game actually passed,
// rather than by my guess about what game paths look like.
//
// The predicates are the three facts this lane established about the callees,
// each traced to where it was established:
//
//   HasExt   -- GetPathFilename (0x1800c6740) strips the extension whether or
//               not its flag is set; the flag ONLY decides what happens when
//               there is no extension, where `true` yields an EMPTY filename.
//               Corroborated by runtime/src/system/filename.cpp:282-288.
//   HasIdent -- GetPathIdentifer (0x1800c6610) returns everything after the
//               last ':'; with no ':' it returns the WHOLE path unless a '.',
//               '/' or '\\' was seen, in which case empty.
//               (runtime/src/system/filename.cpp:366-370.)
//   HasDir   -- GetPathDirectory yields empty when there is no separator.
inline bool HasDirTerm(const char* p) {
    return std::strchr(p, '/') != nullptr || std::strchr(p, 0x5C) != nullptr;
}
inline bool HasExt(const char* p) {
    const char* d = std::strrchr(p, '.');
    return d != nullptr && (d - p) >= 1;   // retail's `dotIndex < 1` test
}
inline bool HasIdent(const char* p) {
    const char* c = std::strrchr(p, ':');
    if (c != nullptr) return c[1] != 0;
    return std::strchr(p, '.') == nullptr && !HasDirTerm(p);
}

// 'X' = this control should change the result for this input, '.' = it should
// not. Derived, never transcribed.
char PredictSensitivity(int ctl, const char* p) {
    switch (ctl) {
        case 0: return '.';                                  // no perturbation
        case 1: return 'X';    // the "_" literal is always concatenated
        case 2: return 'X';    // the ".crs" literal is always concatenated
        // dir <-> fname swap is a no-op when EITHER term is empty. The filename
        // term is empty exactly when the path has no extension, by HasExt above.
        case 3: return (HasDirTerm(p) && HasExt(p)) ? 'X' : '.';
        // flipping GetPathFilename's flag is dead unless there is no extension.
        case 4: return HasExt(p) ? '.' : 'X';
        // substituting the extension for the identifier is a no-op only when
        // both are empty.
        case 5: return (HasExt(p) || HasIdent(p)) ? 'X' : '.';
        default: return '?';
    }
}

const int kSweepN = (int)(sizeof(kSweepPaths) / sizeof(kSweepPaths[0]));

bool g_selfTestPending = false;

void RunCompiledSweep(const Filename* livePath) {
    typedef Filename* (*GcpFn)(Filename*, const Filename*);
    GcpFn theirFn = RtOriginal<GcpFn>(
        "?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z");
    if (theirFn == nullptr) {
        std::printf("[Compiled] SWEEP SKIPPED -- the shipped GetCompiledPath did "
                    "not resolve; NO COMPARISON WAS MADE\n");
        return;
    }

    const CompiledCounters saved = g_cc;   // leave no trace in the live counters

    for (int ctl = 0; ctl <= 5; ++ctl) {
        int compared = 0, mismatches = 0;
        // One character per input: '.' insensitive, 'X' diverged, '-' skipped.
        char mask[32];
        for (int m = 0; m < 32; ++m) mask[m] = 0;
        for (int i = 0; i <= kSweepN; ++i) {
            Filename in{nullptr};
            mask[i] = '.';
            if (i == kSweepN) {
                // case N is the LIVE path this self-test is riding on: the one
                // input we know the game actually produces.
                if (livePath == nullptr || livePath->data == nullptr) {
                    mask[i] = '-';
                    continue;
                }
                RtString_AdoptInline(AsStr(&in), livePath->data);
            } else {
                RtSystem::RtSystem_Filename_CtorCStr(&in, kSweepPaths[i]);
            }

            Filename ours{nullptr}, theirs{nullptr};
            g_ccNegCtl = ctl;
            RtObject_ExternalFactory_GetCompiledPath(&ours, &in);
            g_ccNegCtl = 0;
            theirFn(&theirs, &in);

            ++compared;
            const bool equal =
                ((ours.data == nullptr) == (theirs.data == nullptr)) &&
                (ours.data == nullptr || std::strcmp(ours.data, theirs.data) == 0);
            if (!equal) {
                ++mismatches;
                mask[i] = 'X';
                if (ctl == 0)
                    std::printf("[Compiled] SWEEP ctl=0 MISMATCH in=%s\n"
                                "[Compiled]   ours = %s\n"
                                "[Compiled] theirs = %s\n",
                                in.data ? in.data : "(null)",
                                ours.data ? ours.data : "(null)",
                                theirs.data ? theirs.data : "(null)");
            }
            RtString_ReleaseInline(AsStr(&theirs));
            RtString_ReleaseInline(AsStr(&ours));
            RtString_ReleaseInline(AsStr(&in));
        }
        mask[kSweepN + 1] = 0;
        // Score the MEASURED mask against the PREDICTED one, per input. A
        // disagreement here is the interesting output of this whole screen: it
        // means the model of what the function does is wrong somewhere, and it
        // names the input that proves it.
        // The predicted mask, COMPUTED over the same inputs in the same order.
        char predbuf[32];
        for (int m = 0; m <= kSweepN && m < 30; ++m) {
            const char* txt = (m == kSweepN)
                ? ((livePath && livePath->data) ? livePath->data : nullptr)
                : kSweepPaths[m];
            predbuf[m] = txt ? PredictSensitivity(ctl, txt) : '-';
        }
        predbuf[kSweepN + 1] = 0;
        const char* pred = predbuf;
        char diff[32];
        int disagreements = 0;
        int d = 0;
        for (int m = 0; m <= kSweepN && m < 30; ++m) {
            const char me = mask[m] ? mask[m] : '-';
            if (me == '-') { diff[d++] = '-'; continue; }   // skipped, not scored
            if (me != pred[m]) { diff[d++] = '!'; ++disagreements; }
            else               { diff[d++] = ' '; }
        }
        diff[d] = 0;
        std::printf("[Compiled] SWEEP ctl=%d compared=%d mismatches=%d  %s\n",
                    ctl, compared, mismatches,
                    ctl == 0 ? (mismatches == 0 ? "PASS" : "*** PORT DEFECT ***")
                             : (mismatches  > 0 ? "control CAUGHT"
                                                : "*** control NOT CAUGHT ***"));
        std::printf("[Compiled]   ctl=%d predicted=%s\n", ctl, pred);
        std::printf("[Compiled]   ctl=%d measured =%s\n", ctl, mask);
        std::printf("[Compiled]   ctl=%d disagree =%s  (%d input(s) contradict "
                    "the model)\n", ctl, diff, disagreements);
        if (compared == 0)
            std::printf("[Compiled] SWEEP ctl=%d MADE NO COMPARISONS -- "
                        "this is not a pass\n", ctl);
    }

    g_cc = saved;
    g_ccNegCtl = 0;
}

}  // namespace

// ============================================================================
// ?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z
//                                                          retail 0x1800bec10
// ============================================================================
// `SA` — a STATIC. There is no `this`. It returns a `System::Filename` BY
// VALUE, so the hidden return slot is the FIRST argument and the `const
// Filename&` is the second: `RCX = ret, RDX = path`, confirmed by the prologue
// (`MOV RDI,RCX ; MOV RBX,RDX` at 0x1800bec2f/0x1800bec32, `MOV RAX,RDI` at
// 0x1800bee98). Getting this wrong is the shipped defect
// reference_hidden_retslot_wild_write.md describes: an fn_* typedef that omits
// the slot is a wild write that still returns the right answer.
Filename* RtObject_ExternalFactory_GetCompiledPath(Filename* ret, const Filename* path) {
    RT_DIFF_BEGIN("Object::ExternalFactory::GetCompiledPath");
    ++g_cc.getCompiledCalls;

    BuildCompiledPath(path, ret);

    RT_DIFF_WORK(true);
    // ---- THE ORACLE --------------------------------------------------------
    // PURE, so a live replay is sound: it reads a const buffer and one
    // singleton's const `dataDirectory`, allocates its own result, and mutates
    // nothing. Ours and theirs write two different fresh slots, so they cannot
    // alias (reference_oracle_must_snapshot_aliased_inputs.md).
    if (rt_diff_on__) {
        Filename theirs{nullptr};
        RtOriginal<Filename* (*)(Filename*, const Filename*)>(
            "?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z")
            (&theirs, path);
        const bool equal =
            ((ret->data == nullptr) == (theirs.data == nullptr)) &&
            (ret->data == nullptr || std::strcmp(ret->data, theirs.data) == 0);
        RT_DIFF_SHADOW(equal, ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RtString_ReleaseInline(AsStr(&theirs));
    }
    return ret;
}

// ============================================================================
// ?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z
//                                                          retail 0x1800bf320
// ============================================================================
// RCX = this, RDX = path, returns `const External*` in RAX (RBX in the body).
//
// Retail, in listing order:
//
//   0x1800bf339  if (!path->data) return null
//   0x1800bf345  if (path->data length == 0) return null        [data-8, NOT -4]
//   0x1800bf359  expanded = ExpandRelativePath(this, path, /*appendIdent=*/true)
//   0x1800bf366  found = Find(expanded)                          *** SEE BELOW
//   0x1800bf376  if (found) { if (this->_loadEnabled) Add(found); return found; }
//   0x1800bf398  compiled = <the six-term concatenation>
//   0x1800bf550  loaded = External::Load(compiled, nullptr)
//   0x1800bf562  if (loaded) { loaded->_path = expanded; AdjustDelimiters;
//                              Add(loaded); return loaded; }
//                return null
//
// *** THE `Find` IT CALLS IS THE PRIVATE ONE. 0x1800be0e0 is
// `?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@AEBVFilename@System@@@Z` —
// note `AEAA`, non-const, returning a MUTABLE `External*`. It is NOT
// 0x1800bdeb0, the public `QEBA...PEBVExternal` that `external.cpp` already
// ports. Both are EXPORTED: `AEAA` means PRIVATE, not unexported, and this DLL
// exports its private members (reference_aeaa_is_private_not_unexported.md).
// Calling the ported public one here would be a different function.
//
// THREE NAME DOORS, ZERO ADDRESS DOORS. `Find`(private), `Add` and
// `External::Load` are all unported today and are reached by MANGLED NAME
// through `RT_EXPORT_ONCE`, so each one follows a later port automatically and
// each is visible to tools/unsatisfied_export_scan.py. None of them is reached
// by address, which is the door class that silently keeps using the DLL
// (reference_bypassed_port_needs_a_source_checker.md).
using FindPrivateFn = External* (*)(ExternalFactory*, const Filename*);
using AddFn         = void (*)(ExternalFactory*, External*);
using ExtLoadFn     = External* (*)(const Filename*, External*);

const External* RtObject_ExternalFactory_FindOrLoad(ExternalFactory* self,
                                                    const Filename* path) {
    RT_DIFF_BEGIN("Object::ExternalFactory::FindOrLoad");
    ++g_cc.findOrLoadCalls;

    // The scheduling seam for BOTH of this lane's self-tests. FindOrLoad is the
    // one body in the lane that a normal run actually drives (lane RRES counted
    // 22,583 of them in a menu-to-gameplay run), and it is the only one holding
    // the LIVE ExternalFactory singleton, which the path-stack probe's part 2
    // needs and must not fake. Neither test runs unless its own env var armed
    // it, and each clears its flag before running so it cannot recurse.
    if (g_selfTestPending) {
        g_selfTestPending = false;
        RunCompiledSweep(path);
    }
    PathStackProbeGate(self);

    // 0x1800bf339 / 0x1800bf345
    if (path->data == nullptr || BufLength(path->data) == 0) {
        ++g_cc.emptyPath;
        RT_DIFF_WORK(false);
        return nullptr;
    }

    Filename expanded{nullptr};                                  // [RBP+0x38]
    RtObject_ExternalFactory_ExpandRelativePath(self, &expanded, path, true);

    External* found = RT_EXPORT_ONCE(FindPrivateFn,
        "?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@AEBVFilename@System@@@Z")
        (self, &expanded);

    const External* result = nullptr;

    if (found != nullptr) {
        // ---- 0x1800bf376: the CACHE HIT -----------------------------------
        ++g_cc.cacheHit;
        if (self->_loadEnabled) {
            ++g_cc.hitReAdded;
            RT_EXPORT_ONCE(AddFn, "?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z")
                (self, found);
        } else {
            ++g_cc.hitNotReAdded;
        }
        result = found;
    } else {
        // ---- 0x1800bf390: the MISS ----------------------------------------
        ++g_cc.cacheMiss;
        Filename compiled{nullptr};                              // [RBP+0x40]
        BuildCompiledPath(&expanded, &compiled);

        // ---- THE ONE INDEPENDENT ORACLE THIS BODY HAS ----------------------
        // The string `compiled` is the whole of the miss path's own work: what
        // follows is three calls into functions this lane does not own. And
        // retail has a SEPARATE, INDEPENDENTLY COMPILED body that builds the
        // same string from the same input — `GetCompiledPath` — so comparing
        // against it is not a self-check. It is the reason the two functions
        // share a file.
        if (rt_diff_on__) {
            Filename theirs{nullptr};
            RtOriginal<Filename* (*)(Filename*, const Filename*)>(
                "?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z")
                (&theirs, &expanded);
            const bool equal =
                ((compiled.data == nullptr) == (theirs.data == nullptr)) &&
                (compiled.data == nullptr ||
                 std::strcmp(compiled.data, theirs.data) == 0);
            RT_DIFF_SHADOW(equal, compiled.data ? compiled.data : "(null)",
                           theirs.data ? theirs.data : "(null)");
            RtString_ReleaseInline(AsStr(&theirs));
        }

        External* loaded = RT_EXPORT_ONCE(ExtLoadFn,
            "?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z")
            (&compiled, nullptr);

        if (loaded != nullptr) {
            ++g_cc.loaded;
            // 0x1800bf562-0x1800bf5a6: `loaded->_path = expanded` written out
            // as retail's release-then-adopt with its own `!=` guard, then
            // AdjustDelimiters over the object's OWN field (not a temporary).
            if (loaded->_path.data != expanded.data) {
                RtString_ReleaseInline(AsStr(&loaded->_path));
                RtString_AdoptInline(AsStr(&loaded->_path), expanded.data);
            }
            RtSystem_Filename_AdjustDelimiters(&loaded->_path);   // 0x1800bf5a6
            RT_EXPORT_ONCE(AddFn, "?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z")
                (self, loaded);                                   // 0x1800bf5b1
            result = loaded;
        } else {
            ++g_cc.loadFailed;
        }
        RtString_ReleaseInline(AsStr(&compiled));
    }

    RtString_ReleaseInline(AsStr(&expanded));
    RT_DIFF_WORK(true);
    // NO WHOLE-BODY SHADOW, and this is a measured limit rather than a shrug.
    // Both branches MUTATE: the hit branch conditionally re-`Add`s (which ORs a
    // bit into the entry's usage mask and bumps the generation bookkeeping),
    // and the miss branch allocates an `External` and inserts it. Running
    // retail's FindOrLoad after ours would insert the same key a second time,
    // and retail's second insert RETURNS EARLY — which makes the comparison
    // vacuously green. That is the exact trap external_set.h records for
    // `KeyMap::RegisterInput`. The compiled-path shadow above is what this body
    // can honestly be held to; see WAVE11_LANE_OBJEXT.md for the rest.
    return result;
}

}  // namespace RtObject

// ============================================================================
// Registration
// ============================================================================
// Global scope, like every other wave's Register function, because
// runtime/src/object/rt_object_port.cpp declares it with a bare
// `extern int RtObjectExternalCompiledRegister();`.
int RtObjectExternalCompiledRegister() {
    using namespace RtObject;
    int n = 0;

    const char* nc = std::getenv("BANISHED_RT_COMPILED_NEGCTL");
    g_ccNegCtl = nc ? std::atoi(nc) : 0;

    // Literal mangled names, NOT constants: tools/runtime_census.py matches
    // `RegisterRuntimeExport("<literal>"` and cannot see through a named
    // constant (reference_census_cannot_see_nonliteral_registrations.md).
    RegisterRuntimeExport(
        "?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z",
        (void*)&RtObject_ExternalFactory_GetCompiledPath); ++n;
    RegisterRuntimeAddr(0x1800bec10, (void*)&RtObject_ExternalFactory_GetCompiledPath);

    RegisterRuntimeExport(
        "?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z",
        (void*)&RtObject_ExternalFactory_FindOrLoad); ++n;
    RegisterRuntimeAddr(0x1800bf320, (void*)&RtObject_ExternalFactory_FindOrLoad);

    // Opt-in, not a kill switch: RtWaveEnabled returns true when the variable
    // is UNSET, which is right for a port and wrong for a probe -- it would arm
    // a sweep on every default run.
    const char* se = std::getenv("BANISHED_RT_COMPILED_SWEEP");
    g_selfTestPending = (se != nullptr && std::strcmp(se, "0") != 0);

    // THE IDENTITY LINE. Nothing else in the tree prints "Compiled".
    std::printf("[Compiled] registered %d exports (GetCompiledPath, FindOrLoad) "
                "negctl=%d\n", n, g_ccNegCtl);
    return n;
}
