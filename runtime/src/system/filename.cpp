// ============================================================================
// runtime/src/system/filename.cpp — System::Filename   (Wave 2, lane A)
// ============================================================================
//
// Byte-faithful port of the decompilation at the retail address named above
// each function (Runtime-x64.dll, image base 0x180000000, read 2026-09-06).
//
// Filename is String plus one thing: every mutating entry point ends in
// AdjustDelimiters, which rewrites '/' to '\\' in place and then collapses
// "\..\" segments. runtime/types/system/Filename.h shows the three pins for
// the shared layout; string.h/string.cpp own the refcount idioms this file
// reuses by pointer cast, exactly as retail does.
//
// ABI: same hidden-return-slot rule as String — see the header comment of
// string.cpp. GetPathFilename and GetPathIdentifer both return a Filename BY
// MEMORY, so their signature is (this in RCX, ret in RDX, args from R8).
//
// DIFFABILITY: the ctors, the dtor, the two operator= overloads and
// AdjustDelimiters all mutate a live object and a shared refcount, so running
// the original as an oracle would mutate twice and, at refcount 1, double
// free. They are undiffed. operator<, GetPathFilename and GetPathIdentifer are
// const on `this` and produce independent results, so they are diffed.
// ============================================================================

#include <cstdio>
#include <cstring>

#include "../../types/system/Filename.h"
#include "../../funcs/rt_port.h"
#include "string.h"

namespace RtSystem {

// Retail passes `this` straight into String methods; so do we.
static inline String* AsString(Filename* f) { return reinterpret_cast<String*>(f); }
static inline const String* AsString(const Filename* f) {
    return reinterpret_cast<const String*>(f);
}

void RtSystem_Filename_AdjustDelimiters(Filename* self);

// ============================================================
// Constructors / destructor — NOT DIFFED
// ============================================================

// ??0Filename@System@@QEAA@XZ  retail 0x180006220
Filename* RtSystem_Filename_Ctor(Filename* self) {
    self->data = nullptr;
    return self;
}

// ??0Filename@System@@QEAA@PEBD@Z  retail 0x180026cf0
Filename* RtSystem_Filename_CtorCStr(Filename* self, const char* src) {
    self->data = nullptr;
    RtSystem_String_Set(AsString(self), src);
    RtSystem_Filename_AdjustDelimiters(self);
    return self;
}

// ??0Filename@System@@QEAA@AEBVString@1@@Z  retail 0x18000c780
// Adopts the String's buffer (no copy) and then normalises. Note that
// AdjustDelimiters can WRITE THROUGH that shared buffer — the '/'->'\\' pass
// is in place — so constructing a Filename from a String mutates the String's
// characters too when the path contained forward slashes. That is retail's
// behaviour and several call sites depend on the resulting Filename and its
// source String staying identical.
Filename* RtSystem_Filename_CtorString(Filename* self, const String* src) {
    self->data = nullptr;
    RtString_AdoptInline(AsString(self), src->data);
    RtSystem_Filename_AdjustDelimiters(self);
    return self;
}

// ??0Filename@System@@QEAA@AEBV01@@Z  retail 0x1800067d0
// Not in this lane's assigned list, but GetPathIdentifer calls it, so it is
// ported (and registered — it is a Filename ctor, and leaving one of the four
// on the DLL is exactly the half-ported class PLAN.md §4.1.5 warns about).
// Retail zeroes, then inlines operator=(const Filename&) — which already ends
// in AdjustDelimiters — and then calls AdjustDelimiters a SECOND time. The
// second call is idempotent, but it is transcribed rather than folded away.
Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other) {
    self->data = nullptr;
    if (self->data != other->data) {
        RtString_ReleaseInline(AsString(self));
        self->data = other->data;
        if (self->data != nullptr)
            ++StringRefCount(self->data);
    }
    RtSystem_Filename_AdjustDelimiters(self);
    RtSystem_Filename_AdjustDelimiters(self);
    return self;
}

// ??1Filename@System@@QEAA@XZ  retail 0x180006980
void RtSystem_Filename_Dtor(Filename* self) {
    RtString_ReleaseInline(AsString(self));
}

// ============================================================
// Assignment — NOT DIFFED
// ============================================================

// ??4Filename@System@@QEAAAEAV01@AEBV01@@Z  retail 0x180006770
Filename* RtSystem_Filename_Assign(Filename* self, const Filename* rhs) {
    if (self->data != rhs->data) {
        RtString_ReleaseInline(AsString(self));
        self->data = rhs->data;
        if (self->data != nullptr)
            ++StringRefCount(self->data);
    }
    RtSystem_Filename_AdjustDelimiters(self);
    return self;
}

// ??4Filename@System@@QEAAAEAV01@AEBVString@1@@Z  retail 0x180026d40
// Byte-identical to the Filename& overload — the two are separate exports of
// the same code shape, not a forwarder.
Filename* RtSystem_Filename_AssignString(Filename* self, const String* rhs) {
    if (self->data != rhs->data) {
        RtString_ReleaseInline(AsString(self));
        self->data = rhs->data;
        if (self->data != nullptr)
            ++StringRefCount(self->data);
    }
    RtSystem_Filename_AdjustDelimiters(self);
    return self;
}

// ============================================================
// operator< — diffable
// ============================================================

// ??MFilename@System@@QEBA_NAEBV01@@Z  retail 0x180026cb0
// Case-INSENSITIVE, unlike String::operator< which is case-sensitive; that is
// the whole reason Filename has its own overload, and it is what makes
// Collection::Set<Filename> treat "Terrain\Grass.rsc" and "terrain\grass.rsc"
// as one key.
//
// Retail calls the CRT's _stricmp. That is locale-sensitive in MSVC; the DLL
// never calls setlocale, so it runs in the "C" locale where _stricmp is plain
// ASCII case folding. Spelled out here rather than calling _stricmp so that
// runtime/src stays free of a CRT whose locale behaviour differs between
// toolchains (PLAN.md §4.1: the portability seam). Only the SIGN is used.
static int StriCmpAscii(const char* a, const char* b) {
    for (;;) {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + 32);
        if (ca != cb)
            return static_cast<int>(ca) - static_cast<int>(cb);
        if (ca == 0)
            return 0;
    }
}

bool RtSystem_Filename_Less(const Filename* self, const Filename* other) {
    RT_DIFF_BEGIN("System::Filename::operator<");
    bool ours;
    if (other->data == nullptr)
        ours = false;
    else if (self->data == nullptr)
        ours = true;
    else
        ours = StriCmpAscii(self->data, other->data) < 0;

    RT_DIFF_SCALAR(ours,
        RtOriginal<bool (*)(const Filename*, const Filename*)>(
            "??MFilename@System@@QEBA_NAEBV01@@Z")(self, other));
    return ours;
}

// ============================================================
// AdjustDelimiters — NOT DIFFED (rewrites the shared buffer in place)
// ============================================================

// ?AdjustDelimiters@Filename@System@@AEAAXXZ  retail 0x1800c6970
//
// PRIVATE (AEAA), not QEAA — the `...@QEAAXXZ` spelling in circulation is not
// an export and resolves to null forever (the failure mode of WAVE1 defect 3).
// It is exported anyway, as everything in this DLL is, so it is registered.
//
// Two phases:
//   1. Every '/' becomes '\\', IN PLACE, through the shared buffer.
//   2. Repeatedly find a "\.." whose ".." is not the tail, and splice out the
//      preceding segment: head = [0 .. lastDelim-1], tail = [i+3 .. len-1].
//
// Two details that read as bugs and are not:
//   * `lastDelim` is initialised ONCE, at 0x1800c69b5, ABOVE the outer loop
//     label at 0x1800c69c0. Every re-scan after a splice therefore starts with
//     the PREVIOUS scan's delimiter index, not -1. Transcribed as written.
//   * Each splice both recurses into AdjustDelimiters and then continues the
//     outer loop. The recursion normally finishes the job and the outer
//     re-scan finds nothing, but the loop is what retail wrote.
// GetLength is re-read from the object on every iteration because the splice
// replaces the buffer.
void RtSystem_Filename_AdjustDelimiters(Filename* self) {
    // Phase 1: '/' -> '\\'
    {
        int i = 0;
        int64_t k = 0;
        int len = RtString_GetLength(AsString(self));
        if (len > 0) {
            do {
                if (self->data[k] == '/')
                    self->data[k] = '\\';
                ++i;
                ++k;
                len = RtString_GetLength(AsString(self));
            } while (i < len);
        }
    }

    int lastDelim = -1;   // see the note above: NOT reset by the outer loop

    for (;;) {
        int i = 0;
        int64_t k = 0;
        if (RtString_GetLength(AsString(self)) - 2 <= 0)
            return;

        const char* base = self->data;   // cached once, as retail caches RBP
        bool collapse = false;

        for (;;) {
            bool isDelim = false;
            if (base[k] == '\\') {
                if (base[k + 1] == '.' && base[k + 2] == '.' && lastDelim >= 1) {
                    if (i + 3 < RtString_GetLength(AsString(self)) - 1) {
                        collapse = true;
                        break;
                    }
                    // ".." runs to the end of the path: nothing to splice, so
                    // this backslash is treated as an ordinary delimiter.
                }
                isDelim = true;
            }
            if (isDelim)
                lastDelim = i;
            ++i;
            ++k;
            if (i >= RtString_GetLength(AsString(self)) - 2)
                return;
        }
        if (!collapse)
            return;

        String tail{nullptr};
        String head{nullptr};
        String spliced{nullptr};

        RtSystem_String_SubString(AsString(self), &tail, i + 3,
                                  RtString_GetLength(AsString(self)) - 1);
        RtSystem_String_SubString(AsString(self), &head, 0, lastDelim - 1);
        RtSystem_String_Concat(&spliced, &head, &tail);
        RtSystem_String_Assign(AsString(self), &spliced);
        RtSystem_Filename_AdjustDelimiters(self);

        RtString_ReleaseInline(&spliced);
        RtString_ReleaseInline(&head);
        RtString_ReleaseInline(&tail);
    }
}

// ============================================================
// Path decomposition — diffable
// ============================================================

// Both of these produce a fresh Filename with its own buffer and touch nothing
// shared, so the original can be run into a private scratch and compared.
namespace {
bool SameText(const Filename* a, const Filename* b) {
    if ((a->data == nullptr) != (b->data == nullptr))
        return false;
    if (a->data == nullptr)
        return true;
    return StringLength(a->data) == StringLength(b->data) &&
           std::strcmp(a->data, b->data) == 0;
}
}  // namespace

// ?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z  retail 0x1800c6740
// Returns the leaf, with the extension stripped when `stripExt` is true.
//
// The `stripExt` flag is misleadingly named by its own logic: when NO '.' is
// found at index >= 1, a true flag returns the EMPTY filename rather than the
// whole leaf. So `GetPathFilename(true)` on an extension-less path yields
// nothing at all.
Filename* RtSystem_Filename_GetPathFilename(const Filename* self, Filename* ret, bool stripExt) {
    RT_DIFF_BEGIN("System::Filename::GetPathFilename");

    const char* d = self->data;
    if (d == nullptr) {
        ret->data = nullptr;
    } else {
        int end = StringLength(d) - 1;

        // Scan back for the extension dot; i == -1 means none was found.
        int i = end;
        int64_t k = end;
        if (end >= 0) {
            do {
                if (d[k] == '.')
                    break;
                --i;
                --k;
            } while (k >= 0);
        }
        bool done = false;
        if (i < 1) {
            if (stripExt) {
                ret->data = nullptr;
                done = true;
            }
        } else {
            end = i - 1;
        }

        if (!done) {
            // Scan back for the last path delimiter; either separator counts,
            // even though phase 1 of AdjustDelimiters should have removed '/'.
            i = end;
            k = end;
            if (end >= 0) {
                do {
                    if (d[k] == '/' || d[k] == '\\')
                        break;
                    --i;
                    --k;
                } while (k >= 0);
            }
            int start = 0;
            if (i >= 0)
                start = i + 1;

            if (end < start) {
                ret->data = nullptr;
            } else {
                String tmp{nullptr};
                RtSystem_String_SubString(AsString(self), &tmp, start, end);
                ret->data = nullptr;
                RtString_AdoptInline(AsString(ret), tmp.data);
                RtSystem_Filename_AdjustDelimiters(ret);
                RtString_ReleaseInline(&tmp);
            }
        }
    }

    // `ret != self`: see the same guard in String::SubString — the oracle must
    // not be handed an input our own run has already overwritten.
    if (rt_diff_on__ && ret != self) {
        Filename theirs{nullptr};
        RtOriginal<Filename* (*)(const Filename*, Filename*, bool)>(
            "?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z")(self, &theirs, stripExt);
        RT_DIFF_SHADOW(SameText(ret, &theirs), ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RtString_ReleaseInline(AsString(&theirs));
    }
    return ret;
}

// ?GetPathIdentifer@Filename@System@@QEBA?AV12@XZ  retail 0x1800c6610
// (the misspelling is retail's own). NOTE the return type: V12 = FILENAME.
// The `...QEBA?AVString@2@XZ` spelling in circulation is not an export.
//
// Returns everything after the last ':' — i.e. the path with its drive or
// package identifier removed. If there is no ':' at all the whole path comes
// back UNLESS a '.', '/' or '\\' was seen on the way, in which case the result
// is empty: an identifier is a bare name, and anything that looks like a path
// or a file with an extension has no identifier.
Filename* RtSystem_Filename_GetPathIdentifer(const Filename* self, Filename* ret) {
    RT_DIFF_BEGIN("System::Filename::GetPathIdentifer");

    const char* d = self->data;
    if (d == nullptr) {
        ret->data = nullptr;
    } else {
        bool sawPathChar = false;
        int i = StringLength(d) - 1;
        int64_t k = i;
        if (i >= 0) {
            do {
                const char c = d[k];
                if (c == ':')
                    break;
                bool isPathChar;
                if (static_cast<unsigned char>(c - 0x2e) < 2)   // '.' (0x2e) or '/' (0x2f)
                    isPathChar = true;
                else
                    isPathChar = (c == '\\');
                sawPathChar = sawPathChar || isPathChar;
                --i;
                --k;
            } while (k >= 0);
        }

        if (i < 0) {
            if (sawPathChar)
                ret->data = nullptr;
            else
                RtSystem_Filename_CtorCopy(ret, self);
        } else {
            const int len = d ? StringLength(d) : 0;
            String tmp{nullptr};
            RtSystem_String_SubString(AsString(self), &tmp, i + 1, len - 1);
            ret->data = nullptr;
            RtString_AdoptInline(AsString(ret), tmp.data);
            RtSystem_Filename_AdjustDelimiters(ret);
            RtString_ReleaseInline(&tmp);
        }
    }

    if (rt_diff_on__ && ret != self) {
        Filename theirs{nullptr};
        RtOriginal<Filename* (*)(const Filename*, Filename*)>(
            "?GetPathIdentifer@Filename@System@@QEBA?AV12@XZ")(self, &theirs);
        RT_DIFF_SHADOW(SameText(ret, &theirs), ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RtString_ReleaseInline(AsString(&theirs));
    }
    return ret;
}

}  // namespace RtSystem

// ============================================================
// Registration
// ============================================================
int RtSystemFilenameRegister() {
    using namespace RtSystem;
    int n = 0;

    RegisterRuntimeExport("??0Filename@System@@QEAA@XZ",   (void*)&RtSystem_Filename_Ctor); ++n;
    RegisterRuntimeExport("??0Filename@System@@QEAA@PEBD@Z", (void*)&RtSystem_Filename_CtorCStr); ++n;
    RegisterRuntimeExport("??0Filename@System@@QEAA@AEBVString@1@@Z",
                          (void*)&RtSystem_Filename_CtorString); ++n;
    RegisterRuntimeExport("??0Filename@System@@QEAA@AEBV01@@Z",
                          (void*)&RtSystem_Filename_CtorCopy); ++n;
    RegisterRuntimeExport("??1Filename@System@@QEAA@XZ",   (void*)&RtSystem_Filename_Dtor); ++n;
    RegisterRuntimeExport("??4Filename@System@@QEAAAEAV01@AEBV01@@Z",
                          (void*)&RtSystem_Filename_Assign); ++n;
    RegisterRuntimeExport("??4Filename@System@@QEAAAEAV01@AEBVString@1@@Z",
                          (void*)&RtSystem_Filename_AssignString); ++n;
    RegisterRuntimeExport("??MFilename@System@@QEBA_NAEBV01@@Z",
                          (void*)&RtSystem_Filename_Less); ++n;
    RegisterRuntimeExport("?AdjustDelimiters@Filename@System@@AEAAXXZ",
                          (void*)&RtSystem_Filename_AdjustDelimiters); ++n;
    RegisterRuntimeExport("?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z",
                          (void*)&RtSystem_Filename_GetPathFilename); ++n;
    RegisterRuntimeExport("?GetPathIdentifer@Filename@System@@QEBA?AV12@XZ",
                          (void*)&RtSystem_Filename_GetPathIdentifer); ++n;

    // The SAME functions by Ghidra address — and here it is not belt-and-braces
    // but REQUIRED. Two of the exe's Filename resolves use mangled names that
    // Runtime-x64.dll does not export and never has:
    //
    //   engine_funcs.cpp:1748  "?AdjustDelimiters@Filename@System@@QEAAXXZ"
    //                          (the real export is AEAA — it is private)
    //   engine_funcs.cpp:3114  "?GetPathIdentifer@Filename@System@@QEBA?AVString@2@XZ"
    //                          (the real export returns V12 = Filename, not String)
    //
    // Both have always resolved to null and both fall through to
    // ResolveAddr(Module::Runtime, 0x1800c6970 / 0x1800c6610), so registering
    // only the export names would leave every one of the eight exe call sites
    // of fn_Filename_AdjustDelimiters — and ProfessionUI's GetPathIdentifer —
    // on the DLL while the census reported them ported.
    RegisterRuntimeAddr(0x180006220, (void*)&RtSystem_Filename_Ctor);
    RegisterRuntimeAddr(0x180026cf0, (void*)&RtSystem_Filename_CtorCStr);
    RegisterRuntimeAddr(0x18000c780, (void*)&RtSystem_Filename_CtorString);
    RegisterRuntimeAddr(0x1800067d0, (void*)&RtSystem_Filename_CtorCopy);
    RegisterRuntimeAddr(0x180006980, (void*)&RtSystem_Filename_Dtor);
    RegisterRuntimeAddr(0x180006770, (void*)&RtSystem_Filename_Assign);
    RegisterRuntimeAddr(0x180026d40, (void*)&RtSystem_Filename_AssignString);
    RegisterRuntimeAddr(0x180026cb0, (void*)&RtSystem_Filename_Less);
    RegisterRuntimeAddr(0x1800c6970, (void*)&RtSystem_Filename_AdjustDelimiters);
    RegisterRuntimeAddr(0x1800c6740, (void*)&RtSystem_Filename_GetPathFilename);
    RegisterRuntimeAddr(0x1800c6610, (void*)&RtSystem_Filename_GetPathIdentifer);

    // CONSEQUENCE FOR standalone/filename_runtime_hook.cpp (PLAN.md §2.2
    // hazard A): that diagnostic resolves 0x1800067d0 / 0x180006980 /
    // 0x180006770 through this same ResolveAddr seam in order to byte-patch
    // them, so with BANISHED_FNHOOK=1 it would now try to detour OUR ports.
    // Its expected-prologue guard makes that a refusal rather than a
    // corruption, and it is default-OFF, but the instrument is now unsound and
    // should be retired: its premise ("every caller, including Runtime-
    // internal ones, routes through the check") stopped being true the moment
    // our callers started going to this file instead.
    printf("[RtPort]   System::Filename: %d exports registered\n", n);
    return n;
}
