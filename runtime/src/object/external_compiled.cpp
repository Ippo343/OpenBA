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

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/setupdir_helpers.h"

namespace RtSystem {
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
int g_negCtl = 0;

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
    RtSystem_Filename_GetPathFilename(path, &fname, g_negCtl != 4);
    Filename dir{nullptr};                                       // [RBP+0x30]
    RtSystem_Filename_GetPathDirectory(path, &dir);

    Filename dataDir{nullptr};                                   // [RBP-0x28]
    RtIO::RtIo_FileSystem_GetDataDirectory(FileSystemSingleton(), &dataDir);

    String t1{nullptr}, t2{nullptr}, t3{nullptr}, t4{nullptr}, t5{nullptr};
    RtSystem_String_Concat(&t1, AsStr(&dataDir),
                           AsStr(g_negCtl == 3 ? &fname : &dir));
    RtSystem_String_Concat(&t2, &t1,
                           AsStr(g_negCtl == 3 ? &dir : &fname));
    RtSystem_String_ConcatCStr(&t3, &t2, g_negCtl == 1 ? "-" : kUnderscore);
    RtSystem_String_Concat(&t4, &t3, AsStr(g_negCtl == 5 ? &ext : &ident));
    RtSystem_String_ConcatCStr(&t5, &t4, g_negCtl == 2 ? ".CRS" : kCrs);

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
    g_negCtl = nc ? std::atoi(nc) : 0;

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

    // THE IDENTITY LINE. Nothing else in the tree prints "Compiled".
    std::printf("[Compiled] registered %d exports (GetCompiledPath, FindOrLoad) "
                "negctl=%d\n", n, g_negCtl);
    return n;
}
